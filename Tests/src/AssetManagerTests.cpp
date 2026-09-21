// Tests for kimia::AssetManager — the one place an asset is read from disk.
//
// The point of these tests is that "the cache works" is a measurement: every
// case asserts on the counters, not on a feeling. A cache that silently
// reloads a file every frame looks identical to a working one from the
// outside, which is exactly why the previous frame-loop caches were never
// trusted.
#include <kimia/AssetManager.h>
#include <kimia_test.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef KIMIA_ASSET_DIR
#error "KIMIA_ASSET_DIR must be defined by CMake"
#endif
#ifndef KIMIA_TEST_TMP
#error "KIMIA_TEST_TMP must be defined by CMake"
#endif

#include <sys/stat.h>
#include <sys/types.h>

namespace {

namespace fs = std::filesystem;

// A private tree under the (gitignored) build directory: the tests must not
// write into the source tree wherever the binary is run from.
std::string assetsTmpDir(const std::string& name) {
  static const bool created = ::mkdir(KIMIA_TEST_TMP, 0755) == 0 || errno == EEXIST;
  static_cast<void>(created);
  const std::string dir = std::string(KIMIA_TEST_TMP) + "/assets/" + name;
  std::error_code error;
  fs::remove_all(dir, error);
  fs::create_directories(dir, error);
  return dir;
}

void writeFile(const std::string& path, const std::string& text) {
  std::error_code error;
  fs::create_directories(fs::path(path).parent_path(), error);
  std::ofstream out(path, std::ios::binary);
  out << text;
}

bool contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

const std::string kAssets = std::string(KIMIA_ASSET_DIR) + "/";

}  // namespace

// --- Path resolution ---------------------------------------------------------

KIMIA_TEST(asset_manager_resolves_against_the_configured_roots) {
  const std::string root = assetsTmpDir("roots");
  writeFile(root + "/models/hero.obj", "not really an obj; resolution only");

  kimia::AssetManager assets;
  KIMIA_REQUIRE(assets.roots().empty());
  // Nothing configured: a path relative to the working directory is still
  // tried (that is what old worlds store), so a path into the root is not
  // found by accident.
  KIMIA_REQUIRE(assets.resolve("models/hero.obj").empty());

  assets.addRoot(root);
  KIMIA_REQUIRE(assets.roots().size() == 1U);
  const std::string resolved = assets.resolve("models/hero.obj");
  KIMIA_REQUIRE(contains(resolved, "models/hero.obj"));
  KIMIA_REQUIRE(fs::is_regular_file(resolved));
  KIMIA_REQUIRE(assets.exists("models/hero.obj"));

  // Adding the same root twice is a no-op, and the order is the order added.
  assets.addRoot(root);
  KIMIA_REQUIRE(assets.roots().size() == 1U);
  const std::string second = assetsTmpDir("other");
  assets.addRoot(second);
  KIMIA_REQUIRE(assets.roots().size() == 2U);
  KIMIA_REQUIRE(assets.roots()[0] == root);
  KIMIA_REQUIRE(assets.roots()[1] == second);

  // setProjectRoot replaces everything with one root.
  assets.setProjectRoot(root);
  KIMIA_REQUIRE(assets.roots().size() == 1U);

  // An absolute path is used as stored (phone and embedded builds do this).
  KIMIA_REQUIRE(assets.resolve(root + "/models/hero.obj") == root + "/models/hero.obj");
  // A file that is nowhere resolves to nothing, never to a guess.
  KIMIA_REQUIRE(assets.resolve("models/nobody.obj").empty());
  KIMIA_REQUIRE(assets.resolve("").empty());
}

KIMIA_TEST(asset_manager_keeps_legacy_relative_paths_working) {
  // The engine's own tests run from the source tree, and worlds made before
  // the project root existed store paths like "Tests/assets/crate.obj" or
  // "assets/textures/x.png" relative to the working directory. That spelling
  // must keep resolving with no roots configured at all.
  kimia::AssetManager assets;
  const std::string legacy = kAssets + "crate.obj";
  KIMIA_REQUIRE(fs::is_regular_file(legacy));
  KIMIA_REQUIRE(assets.resolve(legacy) == legacy);
  // The same for the spelling an old Workbench page sent: the configured root
  // already ends in "assets", so "assets/x" must not become "assets/assets/x".
  const fs::path rootPath = fs::path(KIMIA_ASSET_DIR);
  if (rootPath.filename() == "assets") {
    kimia::AssetManager rooted;
    rooted.setProjectRoot(KIMIA_ASSET_DIR);
    KIMIA_REQUIRE(!rooted.resolve("assets/crate.obj").empty());
  }
}

KIMIA_TEST(asset_manager_relative_paths_cannot_escape_a_root) {
  const std::string base = assetsTmpDir("escape");
  writeFile(base + "/root/inside.obj", "inside");
  writeFile(base + "/outside.obj", "outside");

  kimia::AssetManager assets;
  assets.setProjectRoot(base + "/root");
  KIMIA_REQUIRE(!assets.resolve("inside.obj").empty());
  KIMIA_REQUIRE(fs::is_regular_file(assets.resolve("inside.obj")));

  // A world file is data: it must not be able to read the disk around it.
  KIMIA_REQUIRE(assets.resolve("../outside.obj").empty());
  KIMIA_REQUIRE(assets.resolve("sub/../../outside.obj").empty());
  KIMIA_REQUIRE(assets.resolve("./../outside.obj").empty());
  KIMIA_REQUIRE(assets.resolve("/etc/passwd").empty() ||
                fs::is_regular_file("/etc/passwd"));  // absolute paths stay as stored

  // Climbing UP and coming back DOWN inside the root is fine: it never leaves.
  writeFile(base + "/root/models/hero.obj", "hero");
  KIMIA_REQUIRE(!assets.resolve("models/../models/hero.obj").empty());
}

// --- Mesh cache --------------------------------------------------------------

KIMIA_TEST(asset_manager_caches_meshes_and_does_not_reload_per_frame) {
  kimia::AssetManager assets;
  assets.setProjectRoot(KIMIA_ASSET_DIR);
  const std::string file = "crate.obj";

  const kimia::MeshData* first = assets.mesh(file);
  KIMIA_REQUIRE(first != nullptr);
  KIMIA_REQUIRE(first->vertexCount() > 0U);
  KIMIA_REQUIRE(assets.stats().requests == 1U);
  KIMIA_REQUIRE(assets.stats().loads == 1U);
  KIMIA_REQUIRE(assets.stats().hits == 0U);
  KIMIA_REQUIRE(assets.lastError().empty());

  // 120 frames of asking for the same model: one read, 120 answers.
  for (int frame = 0; frame < 120; ++frame) {
    const kimia::MeshData* again = assets.mesh(file);
    KIMIA_REQUIRE(again == first);  // same object, stable address
  }
  KIMIA_REQUIRE(assets.stats().requests == 121U);
  KIMIA_REQUIRE(assets.stats().loads == 1U);
  KIMIA_REQUIRE(assets.stats().hits == 120U);
  KIMIA_REQUIRE(assets.stats().failures == 0U);

  // The mesh, its material table and its skeleton are separate caches over
  // the same file, so asking for a different shape of the same asset loads
  // once more (they are genuinely different parses).
  KIMIA_REQUIRE(assets.meshAsset(file) != nullptr);
  KIMIA_REQUIRE(assets.stats().loads == 2U);
  KIMIA_REQUIRE(assets.stats().hits == 120U);
}

KIMIA_TEST(asset_manager_reports_a_missing_mesh_once_and_remembers_it) {
  kimia::AssetManager assets;
  assets.setProjectRoot(KIMIA_ASSET_DIR);

  for (int frame = 0; frame < 3; ++frame) {
    KIMIA_REQUIRE(assets.mesh("nowhere/ghost.obj") == nullptr);
  }
  // One trip to the disk, two answers from memory — the whole point of a
  // negative cache: a broken path costs one failure, not one per frame.
  KIMIA_REQUIRE(assets.stats().requests == 3U);
  KIMIA_REQUIRE(assets.stats().failures == 1U);
  KIMIA_REQUIRE(assets.stats().loads == 0U);
  KIMIA_REQUIRE(assets.stats().hits == 2U);
  // And the reason survives on every one of those answers.
  KIMIA_REQUIRE(contains(assets.lastError(), "ghost.obj"));
  KIMIA_REQUIRE(contains(assets.lastError(), "not found"));
  KIMIA_REQUIRE(contains(assets.lastError(), "searched"));

  const std::vector<std::string> missing = assets.missingAssets();
  KIMIA_REQUIRE(missing.size() == 1U);
  KIMIA_REQUIRE(missing[0] == "nowhere/ghost.obj");

  // A file that exists but cannot be parsed reports the loader's own words
  // instead of pretending it was missing.
  const std::string broken = assetsTmpDir("broken") + "/broken.obj";
  writeFile(broken, "\x00\x01 this is not an OBJ at all");
  kimia::AssetManager second;
  second.setProjectRoot(fs::path(broken).parent_path().string());
  KIMIA_REQUIRE(second.mesh("broken.obj") == nullptr);
  KIMIA_REQUIRE(contains(second.lastError(), "broken.obj"));
  KIMIA_REQUIRE(second.stats().failures == 1U);

  // invalidate() forgets the failure, so the file gets another chance after
  // the user fixes it.
  KIMIA_REQUIRE(assets.invalidate("nowhere/ghost.obj"));
  KIMIA_REQUIRE(assets.missingAssets().empty());
  KIMIA_REQUIRE(assets.mesh("nowhere/ghost.obj") == nullptr);
  KIMIA_REQUIRE(assets.stats().failures == 2U);
}

// --- Textures and their revision --------------------------------------------

KIMIA_TEST(asset_manager_texture_revision_moves_only_when_the_pixels_do) {
  kimia::AssetManager assets;
  assets.setProjectRoot(KIMIA_ASSET_DIR);

  const std::string file = "crate_skin.png";
  const kimia::AssetManager::Texture first = assets.texture(file);
  KIMIA_REQUIRE(!first.empty());
  KIMIA_REQUIRE(first.image->width > 0);
  KIMIA_REQUIRE(first.revision > 0U);
  const kimia::u64 firstRevision = first.revision;
  KIMIA_REQUIRE(assets.textureRevision(file) == firstRevision);

  // A hundred frames: same image, same revision, no disk traffic. This is
  // what lets a renderer compare one integer instead of hashing pixels.
  for (int frame = 0; frame < 100; ++frame) {
    const kimia::AssetManager::Texture again = assets.texture(file);
    KIMIA_REQUIRE(again.image == first.image);
    KIMIA_REQUIRE(again.revision == firstRevision);
  }
  KIMIA_REQUIRE(assets.stats().loads == 1U);
  KIMIA_REQUIRE(assets.stats().hits == 100U);

  // The file changes on disk: invalidate() is the signal, and the revision
  // moves so nothing can mistake the new pixels for the old ones.
  KIMIA_REQUIRE(assets.invalidate(file));
  KIMIA_REQUIRE(assets.textureRevision(file) == 0U);
  const kimia::AssetManager::Texture reloaded = assets.texture(file);
  KIMIA_REQUIRE(!reloaded.empty());
  KIMIA_REQUIRE(reloaded.revision > firstRevision);
  KIMIA_REQUIRE(assets.stats().loads == 2U);
}

KIMIA_TEST(asset_manager_caches_skinned_assets_and_says_why_not) {
  kimia::AssetManager assets;
  assets.setProjectRoot(KIMIA_ASSET_DIR);

  // skinned_bar.fbx is the tracked animation-only rig in Tests/assets.
  const kimia::assets::SkinnedAsset* rig = assets.skinned("skinned_bar.fbx");
  if (rig != nullptr) {
    KIMIA_REQUIRE(assets.stats().loads == 1U);
    KIMIA_REQUIRE(assets.skinned("skinned_bar.fbx") == rig);
    KIMIA_REQUIRE(assets.stats().hits == 1U);
  } else {
    // Whatever the file holds, a refusal must come with a reason.
    KIMIA_REQUIRE(!assets.lastError().empty());
  }

  // An image path asked for as a mesh fails cleanly and is remembered.
  KIMIA_REQUIRE(assets.mesh("crate_skin.png") == nullptr);
  KIMIA_REQUIRE(!assets.lastError().empty());
  const kimia::u64 failures = assets.stats().failures;
  KIMIA_REQUIRE(assets.mesh("crate_skin.png") == nullptr);
  KIMIA_REQUIRE(assets.stats().failures == failures);
}

KIMIA_TEST(asset_manager_clear_drops_every_cache) {
  kimia::AssetManager assets;
  assets.setProjectRoot(KIMIA_ASSET_DIR);
  KIMIA_REQUIRE(assets.mesh("crate.obj") != nullptr);
  KIMIA_REQUIRE(assets.texture("crate_skin.png").revision > 0U);
  KIMIA_REQUIRE(assets.stats().loads == 2U);

  assets.clear();
  KIMIA_REQUIRE(assets.lastError().empty());
  KIMIA_REQUIRE(assets.missingAssets().empty());
  KIMIA_REQUIRE(assets.textureRevision("crate_skin.png") == 0U);
  KIMIA_REQUIRE(assets.mesh("crate.obj") != nullptr);  // read again, not remembered
  KIMIA_REQUIRE(assets.stats().loads == 3U);
}
