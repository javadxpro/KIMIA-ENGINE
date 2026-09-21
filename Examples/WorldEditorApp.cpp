// KIMIA World — the option-driven editor / object builder (spec section 8).
//
//   kimia_world [--desktop] [--port N] [--world <file.kimia>] [--assets DIR] [--profiles DIR]
//
// Start with an EMPTY ground and build your game with menus only: add a
// player, a ball, blocks, walls, goals — each object asks a few plain
// questions («دقیق باشه یا فانتزی؟») — then manage them (move/delete/color)
// and press PLAY. Worlds save as SceneIO-v1-compatible text.
#include <kimia/Audio.h>
#include <kimia/BitmapFont.h>
#include <kimia/Engine.h>
#include <kimia/Image.h>
#include <kimia/MathUtils.h>
#include <kimia/Mesh.h>
#include <kimia/Renderer.h>
#include <kimia/RuntimeLoop.h>
#include <kimia/WebViewer.h>
#include <kimia/AssetManager.h>
#include <kimia/AssetPipeline.h>
#include <kimia/CameraController.h>
#include <kimia/GameplayEvents.h>
#include <kimia/InputRouter.h>
#include <kimia/OrbitCamera.h>
#include <kimia/RenderSceneBuilder.h>
#include <kimia/Hud.h>
#include <kimia/Input.h>
#include <kimia/Particles.h>
#include <kimia/Picking.h>
#include <kimia/Skeleton.h>
#include <kimia/Studio.h>
#include <kimia/Version.h>
#include <kimia/World.h>
#include <kimia/WorldServer.h>

#ifdef KIMIA_EMBEDDED_ASSETS
#include <kimia/EmbeddedAssets.h>
#endif

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <mutex>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/stat.h>  // ::mkdir for the GPU probe's XDG_RUNTIME_DIR handoff
#endif

using kimia::Engine;
using kimia::EngineOptions;
using kimia::EntityData;
using kimia::GamepadButton;
using kimia::Image;
using kimia::MouseButton;
using kimia::Key;
using kimia::Mat4;
using kimia::MeshData;
using kimia::ObjectKind;
using kimia::RenderScene;
using kimia::Renderer;
using kimia::Vec3;
using kimia::WorldEditor;
using kimia::clamp;
using kimia::f64;
using kimia::i32;
using kimia::u8;
using kimia::u16;
using kimia::usize;

namespace {

std::atomic<bool> running{true};

void onSignal(int) { running.store(false); }

// --gpuinfo: a standalone GPU probe (no window, no server). It reports
// exactly what the engine's renderer will do on this machine — hardware GL
// when libGL + a headless EGL context come up, the software rasteriser
// otherwise. This is the one command to run on a PS4/headless box to see
// whether the (patched) Mesa/amdgpu stack is actually live.
int runGpuInfo() {
  // Mirror Engine::initialize's Unix handoff: a private XDG_RUNTIME_DIR is
  // what some Mesa/EGL platforms expect before they will surface a display.
#ifndef _WIN32
  if (std::getenv("XDG_RUNTIME_DIR") == nullptr) {
    static const char* kRuntimeDir = "/tmp/kimia-xdg";
    ::mkdir(kRuntimeDir, 0700);
    ::setenv("XDG_RUNTIME_DIR", kRuntimeDir, 1);
  }
#endif

  kimia::GLFunctions& gl = kimia::GLFunctions::instance();
  if (!gl.loaded()) gl.load();
  const bool hasLibGL = gl.loaded();

  kimia::EGLContext egl;
  const bool hasEgl = egl.create(640, 480);

  std::printf("KIMIA GPU probe (engine %s)\n", kimia::kEngineVersion);
  std::printf("  libGL loaded      : %s\n", hasLibGL ? "yes" : "no");
  std::printf("  EGL context (3.3) : %s\n", hasEgl ? "yes" : "no");

  if (!hasEgl) {
    std::printf("  renderer          : SOFTWARE (no hardware GL available)\n");
    std::printf("  verdict           : the GPU is not reachable from this kernel/userspace.\n");
    std::printf("                      (a PS4 needs the patched amdgpu/radeon kernel driver\n");
    std::printf("                       AND a Mesa build that knows the Liverpool chip; see\n");
    std::printf("                       Documentation/PS4.md and Tools/ps4_gpu.sh)\n");
    return 1;
  }

  auto read = [&gl](kimia::GLenum name) {
    const kimia::GLchar* s = gl.getString(name);
    return s != nullptr ? std::string(s) : std::string("(unknown)");
  };
  std::printf("  GL vendor         : %s\n", read(kimia::GL_VENDOR).c_str());
  std::printf("  GL renderer       : %s\n", read(kimia::GL_RENDERER).c_str());
  std::printf("  GL version        : %s\n", read(kimia::GL_VERSION).c_str());
  std::printf("  GLSL version      : %s\n", read(kimia::GL_SHADING_LANGUAGE_VERSION).c_str());
  std::printf("  renderer          : HARDWARE OpenGL (the engine's GL path)\n");
  std::printf("  verdict           : GPU is live — kimia_world will use it.\n");
  return 0;
}

// HUD palette, sizes and margins. The shapes and figures that used to sit
// here moved into Engine/View (RenderSceneBuilder); these four are the
// app's own presentation of the engine's HUD lines.
const Vec3 kHudText{1.0, 1.0, 1.0};
const Vec3 kHudBackdrop{0.0, 0.0, 0.0};
const Vec3 kPowerFill{1.0, 0.55, 0.1};
const Vec3 kPowerBack{0.15, 0.15, 0.15};
constexpr i32 kHudScale = 2;   // 10x14 pixel glyphs
constexpr i32 kHudMargin = 8;

// The on-frame HUD: game lines top-left, the power meter bottom-centre.
// Drawn into the captured frame, so it looks the same on GL and software.
void drawHud(Image& image, const WorldEditor& editor) {
  const std::vector<std::string> lines = editor.hudLines();
  if (!lines.empty()) {
    i32 widest = 0;
    for (const std::string& line : lines) widest = std::max(widest, kimia::font::textWidth(line, kHudScale));
    const i32 lineStep = kimia::font::textHeight(kHudScale) + kHudScale * 2;
    const i32 boxWidth = widest + kHudMargin * 2;
    const i32 boxHeight = static_cast<i32>(lines.size()) * lineStep + kHudMargin * 2 - kHudScale * 2;
    kimia::font::fillRect(image, kHudMargin, kHudMargin, boxWidth, boxHeight, kHudBackdrop, 0.55);
    for (usize i = 0; i < lines.size(); ++i) {
      kimia::font::drawText(image, kHudMargin * 2, kHudMargin * 2 + static_cast<i32>(i) * lineStep, lines[i],
                            kHudText, kHudScale);
    }
  }
  const f64 power = editor.hudPower();
  if (power >= 0.0) {
    const i32 barWidth = std::min(240, image.width - kHudMargin * 2);
    const i32 barHeight = 14;
    const i32 labelHeight = kimia::font::textHeight(kHudScale);
    const i32 x = (image.width - barWidth) / 2;
    const i32 y = image.height - kHudMargin - barHeight;
    kimia::font::fillRect(image, x - 4, y - 8 - labelHeight, barWidth + 8, barHeight + labelHeight + 12, kHudBackdrop,
                          0.55);
    kimia::font::drawText(image, x, y - 4 - labelHeight, "POWER", kHudText, kHudScale);
    kimia::font::drawBar(image, x, y, barWidth, barHeight, power, kPowerFill, kPowerBack);
  }
}

// The sound bank comes from the engine (GameplayEvents.h) so the cues an
// event raises and the sounds the app registers cannot drift apart.
void registerSounds(kimia::web::Server& server) {
  for (const auto& cue : kimia::gameplaySoundBank()) server.registerSound(cue.first, cue.second);
}

}  // namespace

namespace {

void printUsage() {
  std::printf(
      "%s — the option-driven game maker (everything is menus)\n"
      "\n"
      "usage: kimia_world [options]\n"
      "  --port N          web port to serve the game on (default 8080)\n"
      "  --bind ADDRESS    WebWorkbench bind address (default 127.0.0.1)\n"
      "  --auth TOKEN      Bearer token required by the remote Workbench\n"
      "  --width N         frame width in pixels (default 640; lower = cooler)\n"
      "  --height N        frame height in pixels (default 480; lower = cooler)\n"
      "  --fps N           frame cap over the web (default 30; lower = cooler)\n"
      "  --quality N       JPEG quality 10..100 (default 80; lower = cooler)\n"
      "  --desktop         open a native Windows/SDL window and use D3D11 when available\n"
      "  --world FILE      world file to save/load (default my_world.kimia)\n"
      "  --assets DIR      OBJ/FBX files you can place in a scene (default assets)\n"
      "  --profiles DIR    *.kimiaprofile game files (default profiles;\n"
      "                    the built-in games always work without this)\n"
      "  --branding DIR    folder with kimia-intro.mp4 / kimia-logo.png\n"
      "                    (default: Branding next to the app or the build)\n"
      "  --no-intro        do not play the intro film\n"
      "  --gpuinfo         probe the GPU (libGL + headless EGL) and report which\n"
      "                    renderer the engine will use, then exit\n"
      "  --version         print the engine version and exit\n"
      "  --help            print this text and exit\n"
      "\n"
      "then open http://127.0.0.1:<port> in a browser and tap the menus.\n",
      kimia::kEngineVersionString);
}

}  // namespace

int main(int argc, char** argv) {
  int port = 8080;
  std::string webBindAddress = "127.0.0.1";
  std::string webAuthToken;
  std::string worldPath = "my_world.kimia";
  std::string assetsDir = "assets";
  std::string profilesDir = "profiles";
  std::string brandingDir;  // empty = look in Branding, ../Branding, ../../Branding
  std::string playWorld;    // non-empty = a published game, not the editor
  bool desktopMode = false;
  int frameWidth = 640;   // software-capture size; the phone runs cooler with less
  int frameHeight = 480;
  int maxFps = 30;        // web frame cap; lower = less CPU, cooler device
  int jpegQuality = 80;   // JPEG encode quality; lower = faster + cooler
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--port" && i + 1 < argc) {
      port = std::atoi(argv[++i]);
    } else if (arg == "--bind" && i + 1 < argc) {
      webBindAddress = argv[++i];
    } else if (arg == "--auth" && i + 1 < argc) {
      webAuthToken = argv[++i];
    } else if (arg == "--desktop") {
      desktopMode = true;
    } else if (arg == "--width" && i + 1 < argc) {
      frameWidth = std::atoi(argv[++i]);
    } else if (arg == "--height" && i + 1 < argc) {
      frameHeight = std::atoi(argv[++i]);
    } else if (arg == "--fps" && i + 1 < argc) {
      maxFps = std::atoi(argv[++i]);
    } else if (arg == "--quality" && i + 1 < argc) {
      jpegQuality = std::atoi(argv[++i]);
    } else if (arg == "--world" && i + 1 < argc) {
      worldPath = argv[++i];
    } else if (arg == "--assets" && i + 1 < argc) {
      assetsDir = argv[++i];
    } else if (arg == "--profiles" && i + 1 < argc) {
      profilesDir = argv[++i];
    } else if (arg == "--branding" && i + 1 < argc) {
      brandingDir = argv[++i];
    } else if (arg == "--play" && i + 1 < argc) {
      // A published game: open this world and go straight into play, with
      // no builder anywhere on screen.
      playWorld = argv[++i];
    } else if (arg == "--no-intro") {
      brandingDir = "-";  // a folder that cannot exist: skips the film
    } else if (arg == "--version") {
      std::printf("%s\n", kimia::kEngineVersionString);
      return 0;
    } else if (arg == "--gpuinfo") {
      return runGpuInfo();
    } else if (arg == "--help" || arg == "-h") {
      printUsage();
      return 0;
    } else {
      // Never a silent no-op: a typo like `--porrt 9000` used to be ignored
      // and the game quietly ran on the wrong port. Say so and stop.
      std::printf("kimia_world: unknown or incomplete option: %s\n\n", arg.c_str());
      printUsage();
      return 2;
    }
  }

  // Clamp the knobs that control CPU load so a typo cannot ask for a
  // 1x1 frame or a 0-fps loop.
  frameWidth = std::max(64, std::min(frameWidth, 4096));
  frameHeight = std::max(64, std::min(frameHeight, 4096));
  maxFps = std::max(5, std::min(maxFps, 60));
  jpegQuality = std::max(10, std::min(jpegQuality, 100));

  WorldServerOptions opts;
  opts.port = port;
  opts.bindAddress = webBindAddress;
  opts.authToken = webAuthToken;
  opts.worldPath = worldPath;
  opts.assetsDir = assetsDir;
  opts.profilesDir = profilesDir;
  opts.brandingDir = brandingDir;
  opts.playWorld = playWorld;
  opts.desktopMode = desktopMode;
  opts.frameWidth = frameWidth;
  opts.frameHeight = frameHeight;
  opts.maxFps = maxFps;
  opts.jpegQuality = jpegQuality;
  return runWorldServer(opts);
}

int runWorldServer(const WorldServerOptions& opts) {
  running.store(true);  // re-arm after a shutdown, so the APK can restart

  const int port = opts.port;
  const std::string webBindAddress = opts.bindAddress;
  const std::string webAuthToken = opts.authToken;
  const std::string worldPath = opts.worldPath;
  std::string assetsDir = opts.assetsDir;
  std::string profilesDir = opts.profilesDir;
  std::string brandingDir = opts.brandingDir;
  const std::string playWorld = opts.playWorld;
  const bool desktopMode = opts.desktopMode;
  const int frameWidth = opts.frameWidth;
  const int frameHeight = opts.frameHeight;
  const int maxFps = opts.maxFps;
  const int jpegQuality = opts.jpegQuality;

#ifdef KIMIA_EMBEDDED_ASSETS
  // Self-contained build (single .exe / .apk): unpack the bundled
  // Profiles/Worlds/Branding once into a writable directory and point the
  // editor there, so the app runs with nothing next to it. CLI defaults are
  // only replaced when the user did not pass an explicit path.
  {
    static std::string embeddedRoot;
    static bool embeddedResolved = false;
    if (!embeddedResolved) {
      embeddedResolved = true;
      std::filesystem::path base = opts.unpackDir.empty()
                                       ? std::filesystem::temp_directory_path()
                                       : std::filesystem::path(opts.unpackDir);
      base /= "kimia_engine";
      base /= kimia::kEngineVersion;
      std::error_code ec;
      const std::filesystem::path sentinel = base / ".complete";
      if (!std::filesystem::exists(sentinel, ec)) {
        ec.clear();
        std::filesystem::remove_all(base, ec);
        ec.clear();
        if (kimia::embedded::extractAll(base.string())) {
          std::ofstream stamp(sentinel, std::ios::binary);
          if (stamp) stamp << kimia::kEngineVersion;
        }
      }
      if (std::filesystem::exists(sentinel, ec)) embeddedRoot = base.string();
    }
    if (!embeddedRoot.empty()) {
      if (assetsDir == "assets") assetsDir = embeddedRoot + "/assets";
      if (profilesDir == "profiles") profilesDir = embeddedRoot + "/profiles";
      if (brandingDir.empty()) brandingDir = embeddedRoot + "/Branding";
    }
  }
#endif

  WorldEditor editor;
  // A published game opens straight into its world; the editor opens on
  // its menu as before.
  bool publishedGame = false;
  // The Workbench API runs on the server's accept thread while this loop
  // is updating the world, so both sides take this lock. Without it a
  // request landing mid-update would be a genuine data race.
  std::mutex editorMutex;
  editor.setWorldPath(worldPath);
  editor.setImportDirectory(assetsDir);
  editor.setProfileDirectory(profilesDir);  // built-ins + *.kimiaprofile files
  if (!playWorld.empty()) {
    std::string startError;
    if (editor.startPublished(playWorld, startError)) {
      publishedGame = true;
    } else {
      std::printf("cannot open the game '%s': %s\n", playWorld.c_str(), startError.c_str());
      return 2;
    }
  } else {
    // The world named on the command line, when there is one: `--world FILE`
    // is documented as "save/load", and this is the load half. It used to be
    // the save path only, so asking the editor to open a world silently
    // opened the demo instead.
    std::string startError;
    bool opened = false;
    if (std::filesystem::exists(worldPath)) {
      opened = editor.loadWorld(worldPath, startError);
      if (!opened) {
        std::printf("cannot open the world '%s': %s\n", worldPath.c_str(), startError.c_str());
      }
    }
    // The Workbench needs a non-empty world for Hierarchy/Inspector/Project
    // to have anything to show. Try the shipped "street kids" demo first;
    // if that is missing (e.g. a freshly-built tree without the embedded
    // assets extracted yet), fall back to a fresh ground + player + ball
    // built from the default "golf" profile so every panel has something.
    const std::string streetDemo =
        (assetsDir == "assets" ? std::string("Worlds/street_kids.kimia")
                               : assetsDir + "/../Worlds/street_kids.kimia");
    if (!opened && !editor.loadWorld(streetDemo, startError)) {
      // builtinProfiles() returns its vector BY VALUE, so the profile has to
      // be copied out of the loop. Holding a pointer into that range-for left
      // it dangling the moment the loop ended, and this fallback — reached
      // whenever Worlds/street_kids.kimia is not next to the asset folder —
      // crashed on startup instead of building the stand-in world.
      kimia::GameProfile golf;
      bool hasGolf = false;
      for (const kimia::GameProfile& p : kimia::builtinProfiles()) {
        if (p.name == "golf") {
          golf = p;
          hasGolf = true;
          break;
        }
      }
      if (hasGolf) {
        editor.createWorld(golf);
        editor.createObject("player", Vec3{0.0, 0.0, 4.0});
        editor.createObject("ball", Vec3{0.0, 0.0, 3.0});
        editor.createObject("hole", Vec3{0.0, 0.0, -2.0});
      }
    }
  }

  EngineOptions options;
  options.headless = !desktopMode;
  options.windowHidden = !desktopMode;
  options.preferD3D11 = desktopMode;
  options.windowWidth = 640;
  options.windowHeight = 480;
  options.enableWeb = true;
  options.webPort = static_cast<u16>(port);
  options.webBindAddress = webBindAddress;
  options.webAuthToken = webAuthToken;
  options.windowTitle = "KIMIA World";
  Engine engine;
  if (!engine.initialize(options)) {
    std::printf("engine init failed\n");
    return 1;
  }
  if (engine.server() == nullptr) {
    std::printf("web server failed to start on %s:%d — the port is probably already in use.\n"
                "  kill the old process (pkill -f kimia_world) or pick another port with --port N.\n",
                webBindAddress.c_str(), port);
    return 1;
  }

  // Physical-keyboard keymap: 1-6 pick options, arrows move, Shift = fine,
  // r/b actions.
  const char* keymapJs =
      "var km={'1':'t:num1','2':'t:num2','3':'t:num3','4':'t:num4','5':'t:num5','6':'t:num6',"
      "'7':'t:num7','8':'t:num8','9':'t:num9',"
      "'r':'t:r','b':'t:b','j':'t:j',' ':'h:space','ArrowUp':'h:up','ArrowDown':'h:down','ArrowLeft':'h:left',"
      "'ArrowRight':'h:right','Shift':'h:shift',"
      "'c':'h:c','q':'h:q','e':'h:e','p':'t:p'};\n"
      "function kmd(e,down){var m=km[e.key];if(!m)return;e.preventDefault();"
      "if(m[0]==='h')post('key='+m.slice(2)+'&down='+(down?1:0));else if(down)post('tap='+m.slice(2));}\n"
      "window.addEventListener('keydown',function(e){kmd(e,true);});\n"
      "window.addEventListener('keyup',function(e){kmd(e,false);});";

  engine.server()->stop();
  if (!engine.server()->start(
      options.webPort,
      kimia::web::makePageHtml(
          publishedGame ? editor.world().name : std::string("KIMIA World"), {}, keymapJs,
          publishedGame
              ? std::string("arrows move, Space = jump, and the pads below are your controls")
              : std::string("everything is menus: tap 1-9 for the options, arrows move, Shift = fine, "
                            "r resets, b opens the menu, Space = jump (or hold to charge a shot), "
                            "hold c = dribble, hold q/e = curl, p = pass"),
          !publishedGame),
      kimia::web::ServerOptions{options.webBindAddress, options.webAuthToken})) {
    std::printf("web server failed to restart on %s:%d\n", options.webBindAddress.c_str(), port);
    return 1;
  }
  // The intro film, if the Branding folder shipped with this build.
  const bool intro = kimia::web::loadIntroFrom(*engine.server(), brandingDir);
  const std::string d3dStatus =
      engine.d3d11Available()
          ? engine.d3d11().adapterName() + " (FL " + engine.d3d11().featureLevelName() + ")"
          : std::string("off");
  // Which GL renderer is actually behind the engine's GL path, so a
  // headless box (PS4) reports its GPU by name in one glance.
  std::string glStatus = "no (software)";
  if (engine.glAvailable()) {
    const kimia::GLchar* s = kimia::GLFunctions::instance().getString(kimia::GL_RENDERER);
    glStatus = s != nullptr && s[0] != '\0' ? std::string("yes (") + s + ")" : std::string("yes");
  }
  std::printf("KIMIA World %s serving on port %d | GL: %s | D3D11: %s | games: %d\n", kimia::kEngineVersion,
              static_cast<i32>(engine.server()->port()), glStatus.c_str(),
              d3dStatus.c_str(), static_cast<i32>(editor.menuProfileCount()));
  std::printf("intro: %s\n", intro ? "yes" : "no (no Branding/kimia-intro.mp4)");

  Renderer renderer;
  std::string rendererError;
  if (engine.glAvailable() && !renderer.initialize(rendererError)) {
    std::printf("renderer init failed: %s\n", rendererError.c_str());
  }
  registerSounds(*engine.server());

  // --- KIMIA Workbench (stage 32) ---
  // The editor page and the API behind it. The server hands requests to
  // the studio layer, which asks the WorldEditor real questions — so
  // every decision stays in the engine where it is tested, and the page
  // is only ever a view of it.
  //
  // The handler runs on the server's accept thread while the main loop is
  // updating the world, so it takes the same lock the frame loop uses.
  // A published game serves NO editor. Leaving /bench reachable would let
  // anyone you gave the game to open the builder and take it apart.
  if (!publishedGame) {
    engine.server()->setPage("/bench", kimia::studio::benchPage());
  }
  engine.server()->setApiHandler(
      [&editor, &editorMutex, publishedGame](const std::string& path,
                                             const std::map<std::string, std::string>& params) {
        // ... and no editing API either, or the page being gone would be
        // cosmetic rather than real.
        if (publishedGame) return std::string("{\"ok\":false,\"error\":\"published game\"}");
        std::lock_guard<std::mutex> lock(editorMutex);
        return kimia::studio::handleApi(editor, path, params);
      });
  engine.server()->setUploadHandler(
      [&editor, &editorMutex, publishedGame](const std::string& /*path*/,
                                             const std::map<std::string, std::string>& params,
                                             const std::string& body) {
        if (publishedGame) return std::string("{\"ok\":false,\"error\":\"published game\"}");
        std::lock_guard<std::mutex> lock(editorMutex);
        return kimia::studio::saveAssetFile(editor, params, body);
      });

  // The frame's three engine-side helpers (see Documentation/Architecture.md):
  // a file read goes through the asset manager, the draw list comes from the
  // scene builder, and the camera rig is the camera controller. The loop
  // itself only moves data between them.
  //
  // The manager is the EDITOR'S (editor.assetManager()), not a second one:
  // importing a model, previewing it in the Workbench and drawing it every
  // frame are then three readers of one cache, so a file is parsed once for
  // the whole session however many of them touch it.
  kimia::RenderSceneBuilder sceneBuilder(editor.assetManager());
  kimia::CameraController cameraController;
  i32 width = frameWidth;
  i32 height = frameHeight;

  std::signal(SIGINT, onSignal);
  const auto frameStart = std::chrono::steady_clock::now();
  auto lastTime = frameStart;
  // Cap the web loop at maxFps. Every frame is a full software raster plus a
  // PNG encode, so the cap is what keeps a phone from cooking itself.
  const std::chrono::microseconds frameBudget(1000000 / maxFps);
  kimia::RuntimeLoop runtimeLoop;
  bool d3dFailed = false;

  while (running.load() && !editor.quitRequested()) {
    const auto now = std::chrono::steady_clock::now();
    f64 dt = static_cast<f64>(std::chrono::duration_cast<std::chrono::microseconds>(now - lastTime).count()) /
             1000000.0;
    lastTime = now;
    dt = clamp(dt, 0.0, 0.1);

    if (!engine.poll()) break;
    if (desktopMode && engine.window() != nullptr) {
      width = std::max<i32>(1, engine.window()->width());
      height = std::max<i32>(1, engine.window()->height());
    }
    kimia::InputState& input = engine.input();

    if (input.pressed(Key::Escape)) break;

    // Guards the world while this frame reads and steps it. It is released
    // before the server is touched below: the server takes its own lock,
    // and the API handler takes this one, so holding both at once here
    // would be a lock-order inversion. It deadlocked the first time it ran.
    std::unique_lock<std::mutex> editorLock(editorMutex);
    // One call turns this frame's keys, mouse and controller into what the
    // world should do (Engine/View/InputRouter). The camera contract it
    // returns is applied to the rig immediately, exactly where the inline
    // version used to orbit and zoom.
    const kimia::RoutedInput routed = kimia::routeEditorInput(editor, input, dt);
    cameraController.applyInput(routed.camera);
    cameraController.update(editor, dt);
    // Hand the camera to the engine so a tap on the picture can be turned
    // into an object. The app owns the camera; the engine owns the decision
    // about what was hit, where it can be tested.
    editor.setViewport(cameraController.viewport(width, height));

    runtimeLoop.pause(editor.playing() && editor.paused());
    runtimeLoop.tick(dt, [&editor](f64 fixedStep) { editor.update(fixedStep); });
    runtimeLoop.beginRenderFrame();
    // Drains the world's events: a component bound to "goal" or "kick" fires
    // (with no game code knowing it exists, stage 31) and the sound cue for
    // each event comes back from the engine's own table.
    for (const char* cue : kimia::pumpGameplayEvents(editor)) engine.server()->playSound(cue);
    // Sounds queued by those components.
    for (const std::string& sound : editor.drainTriggeredSounds()) engine.server()->playSound(sound);

    // --- Build the frame ---
    // The frame's clear colour, then the draw list from the engine's scene
    // builder (models, materials, textures, posed characters, ghosts,
    // markers): a file read goes through the asset manager and never through
    // this loop.
    const kimia::EnvironmentColors colors = kimia::environmentColors(editor.world().environment);
    RenderScene scene;
    sceneBuilder.build(editor, scene);
    cameraController.applyTo(scene, width, height);

    // The PC path renders directly to the native swap chain. The web view
    // still receives a software/GL capture below, so hybrid editing works
    // without making the browser the primary display.
    if (!d3dFailed && engine.d3d11Available()) {
      std::string d3dError;
      if (!engine.d3d11().render(scene, width, height, d3dError)) {
        std::printf("D3D11 frame failed; falling back to software: %s\n", d3dError.c_str());
        d3dFailed = true;
      }
    }
    Image image;
    if (renderer.ready()) {
      renderer.render(scene, width, height);
      if (!renderer.captureImage(width, height, image)) image = Image{};
    }
    if (image.isEmpty()) kimia::renderSoftware(scene, width, height, colors.clear, image);
    drawHud(image, editor);
    // The interface the USER laid out, over the engine's own corner text.
    // Drawn second so a panel can deliberately sit on top of it.
    kimia::drawHud(image, editor.hud(), editor.logic());

    // The game's own on-screen controls. Drawn last so a finger always
    // has something to aim at, whatever else is on screen.
    if (editor.playing()) {
      const kimia::InputMap& controls = editor.input();
      const i32 shortSide = image.width < image.height ? image.width : image.height;
      for (const kimia::Control* control : controls.touchControls()) {
        const i32 radius = static_cast<i32>(control->spot.size * 0.5 * static_cast<f64>(shortSide));
        const i32 cx = static_cast<i32>(control->spot.x * static_cast<f64>(image.width));
        const i32 cy = static_cast<i32>(control->spot.y * static_cast<f64>(image.height));
        kimia::font::fillRect(image, cx - radius, cy - radius, radius * 2, radius * 2,
                              Vec3{0.12, 0.16, 0.22}, 0.65);
        const std::string label = control->spot.label.empty() ? control->name : control->spot.label;
        const i32 textW = kimia::font::textWidth(label, 2);
        kimia::font::drawText(image, cx - textW / 2, cy - kimia::font::textHeight(2) / 2, label,
                              Vec3{0.95, 0.95, 1.0}, 2);
      }
      if (controls.showStick) {
        const i32 radius = static_cast<i32>(controls.stickSpot.size * 0.5 * static_cast<f64>(shortSide));
        const i32 cx = static_cast<i32>(controls.stickSpot.x * static_cast<f64>(image.width));
        const i32 cy = static_cast<i32>(controls.stickSpot.y * static_cast<f64>(image.height));
        kimia::font::fillRect(image, cx - radius, cy - radius, radius * 2, radius * 2,
                              Vec3{0.10, 0.13, 0.18}, 0.55);
        kimia::font::fillRect(image, cx - radius / 3, cy - radius / 3, (radius / 3) * 2, (radius / 3) * 2,
                              Vec3{0.35, 0.45, 0.6}, 0.9);
      }
    }
    if (desktopMode && engine.window() != nullptr && (d3dFailed || !engine.d3d11Available())) {
      engine.window()->present(image);
    }
    std::vector<u8> jpg = image.encodeJPG(jpegQuality);

    // --- Menu (buttons the user sees) ---
    kimia::web::Menu menu;
    menu.title = editor.menuTitle();
    const std::vector<std::string> labels = editor.optionLabels();
    for (usize i = 0; i < labels.size(); ++i) {
      menu.taps.push_back({labels[i], "num" + std::to_string(i + 1U)});
    }
    for (const auto& pad : editor.holdPad()) menu.holds.push_back({pad.first, pad.second});
    for (const auto& pad : editor.tapPad()) menu.taps.push_back({pad.first, pad.second});

    const std::string stats = editor.statsLine();
    editorLock.unlock();  // never hold the world lock while calling the server
    engine.server()->publishFrame(std::move(jpg), stats);
    engine.server()->setMenu(menu);
    engine.endFrame();

    const auto elapsed = std::chrono::steady_clock::now() - now;
    const auto left = frameBudget - std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
    if (left.count() > 0) std::this_thread::sleep_for(left);
  }

  std::printf("bye | %s\n", editor.statsLine().c_str());
  return 0;
}

void requestWorldServerShutdown() { running.store(false); }
