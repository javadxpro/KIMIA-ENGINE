// Tests for Engine/View: the pieces the frame loop used to do inline —
// input routing, the camera rig, the draw-list builder and the gameplay-event
// table. None of these need a window, a GL context or the software
// rasteriser, which is the whole reason they are in the engine.
#include <kimia/CameraController.h>
#include <kimia/GameplayEvents.h>
#include <kimia/InputRouter.h>
#include <kimia/RenderSceneBuilder.h>
#include <kimia/SceneIO.h>
#include <kimia/MathUtils.h>
#include <kimia_test.h>

#include <sys/stat.h>

#ifndef KIMIA_ASSET_DIR
#error "KIMIA_ASSET_DIR must be defined by CMake"
#endif
#ifndef KIMIA_TEST_TMP
#error "KIMIA_TEST_TMP must be defined by CMake"
#endif

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

namespace {

using kimia::AssetManager;
using kimia::CameraController;
using kimia::CameraInput;
using kimia::EntityData;
using kimia::GamepadAxis;
using kimia::GamepadButton;
using kimia::InputState;
using kimia::Key;
using kimia::MouseButton;
using kimia::RenderScene;
using kimia::RenderSceneBuilder;
using kimia::RoutedInput;
using kimia::Vec3;
using kimia::WorldEditor;
using kimia::f64;

// The editor walked into the builder with the empty sandbox world, the same
// way Tests/src/WorldTests.cpp does it.
WorldEditor editorWithWorld() {
  WorldEditor editor;
  editor.choose(0);  // Main -> «کدام بازی؟»
  editor.choose(0);  // ground -> Builder
  return editor;
}

void createWorldFor(WorldEditor& editor, const char* profileName) {
  for (kimia::usize i = 0; i < editor.profileCount(); ++i) {
    if (editor.profileAt(i).name == profileName) {
      editor.createWorld(editor.profileAt(i));
      return;
    }
  }
}

bool near(f64 a, f64 b, f64 eps = 1e-9) { return std::abs(a - b) <= eps; }

// Every mesh a frame draws, in order. Two frames of an unchanged world must
// produce the same pointers: that is what "the frame copies no geometry"
// means, and it is the property the renderer's own caches rely on.
std::vector<const kimia::MeshData*> meshPointers(const RenderScene& scene) {
  std::vector<const kimia::MeshData*> pointers;
  pointers.reserve(scene.objects.size());
  for (const kimia::RenderObject& object : scene.objects) pointers.push_back(object.mesh);
  return pointers;
}

const std::string kAssets = std::string(KIMIA_ASSET_DIR) + "/";

// Test outputs go into the (gitignored) build directory, never into the
// source tree, wherever the binary is run from.
std::string tmpPath(const std::string& name) {
  const int made = ::mkdir(KIMIA_TEST_TMP, 0755);
  static_cast<void>(made);
  return std::string(KIMIA_TEST_TMP) + "/" + name;
}

}  // namespace

// --- Gameplay events ---------------------------------------------------------

KIMIA_TEST(view_gameplay_event_table_covers_every_event) {
  using kimia::WorldEditor;
  const std::vector<kimia::GameplayEventInfo>& table = kimia::gameplayEvents();
  KIMIA_REQUIRE(table.size() == 8U);  // one per GameEvent in World.h

  // Every entry answers both questions, and the two names are never empty —
  // an empty trigger would match every component with an empty trigger field.
  std::vector<std::string> cues;
  for (const kimia::GameplayEventInfo& info : table) {
    KIMIA_REQUIRE(info.trigger != nullptr && info.trigger[0] != '\0');
    KIMIA_REQUIRE(info.cue != nullptr && info.cue[0] != '\0');
    KIMIA_REQUIRE(std::string(kimia::soundCueFor(info.event)) == info.cue);
    KIMIA_REQUIRE(std::string(kimia::triggerFor(info.event)) == info.trigger);
    // The engine's own trigger naming is what the editor shows the user, so
    // the table must not invent a second spelling.
    KIMIA_REQUIRE(std::string(WorldEditor::eventTriggerName(info.event)) == info.trigger);
    cues.push_back(info.cue);
  }
  // Distinct cues: two events playing the same sound would make a goal sound
  // like a kick.
  std::sort(cues.begin(), cues.end());
  KIMIA_REQUIRE(std::adjacent_find(cues.begin(), cues.end()) == cues.end());

  // The sound bank has exactly one entry per cue, so registering the bank
  // cannot leave an event silent.
  const auto bank = kimia::gameplaySoundBank();
  KIMIA_REQUIRE(bank.size() == cues.size());
  for (const auto& entry : bank) {
    KIMIA_REQUIRE(!entry.first.empty());
    KIMIA_REQUIRE(!entry.second.empty());  // real WAV bytes
    KIMIA_REQUIRE(std::find(cues.begin(), cues.end(), entry.first) != cues.end());
  }
}

KIMIA_TEST(view_pump_delivers_event_cues_and_fires_component_triggers) {
  // A golf world with a ball, a sound component bound to the "shot" trigger,
  // and one real shot: the pump must hand back the cue AND make the component
  // fire, which is what connects an event to a sound and to an animation with
  // no game code in between.
  WorldEditor editor;
  createWorldFor(editor, "golf");
  KIMIA_REQUIRE(editor.hasWorld());
  KIMIA_REQUIRE(kimia::pumpGameplayEvents(editor).empty());  // nothing happened yet

  editor.choose(0);  // catalog
  editor.choose(1);  // ball -> Place
  editor.setGhostPosition(Vec3{0.0, 0.0, 3.0});
  editor.choose(0);  // place the ball
  editor.choose(1);  // back to the builder
  KIMIA_REQUIRE(editor.addEntitySound("Ground", kimia::SoundComponent{"kick", "shot", 1.0}));

  editor.choose(3);  // PLAY
  KIMIA_REQUIRE(editor.playing());
  KIMIA_REQUIRE(editor.drainTriggeredSounds().empty());

  editor.setShootHeld(true);
  editor.update(0.5);
  editor.setShootHeld(false);
  editor.update(1.0 / 120.0);  // the release fires the shot

  const std::vector<const char*> cues = kimia::pumpGameplayEvents(editor);
  KIMIA_REQUIRE(cues.size() == 1U);
  KIMIA_REQUIRE(std::string(cues[0]) == "shot");
  // The component wired to "shot" fired on its own.
  const std::vector<std::string> triggered = editor.drainTriggeredSounds();
  KIMIA_REQUIRE(triggered.size() == 1U);
  KIMIA_REQUIRE(triggered[0] == "kick");
  // Draining is once: a second pump finds nothing and fires nothing.
  KIMIA_REQUIRE(kimia::pumpGameplayEvents(editor).empty());
  KIMIA_REQUIRE(editor.drainTriggeredSounds().empty());
}

// --- Input routing -----------------------------------------------------------

KIMIA_TEST(view_input_router_moves_the_player_and_reports_the_keys) {
  WorldEditor editor = editorWithWorld();
  InputState input;

  // Arrows drive movement intent, and Shift means "fine".
  input.setKeyDown(Key::Right, true);
  input.setKeyDown(Key::Up, true);
  input.setKeyDown(Key::Shift, true);
  RoutedInput routed = kimia::routeEditorInput(editor, input, 1.0 / 60.0);
  KIMIA_REQUIRE(near(routed.moveX, 1.0));
  KIMIA_REQUIRE(near(routed.moveZ, -1.0));
  KIMIA_REQUIRE(routed.fine);
  // The frame the keys go down they are an edge: their names reach the logic
  // (so a rule saying "when key right" fires) and the input map.
  KIMIA_REQUIRE(std::find(routed.pressedKeys.begin(), routed.pressedKeys.end(), "right") !=
                routed.pressedKeys.end());
  KIMIA_REQUIRE(std::find(routed.pressedKeys.begin(), routed.pressedKeys.end(), "up") !=
                routed.pressedKeys.end());
  KIMIA_REQUIRE(routed.moveX == 1.0);

  // The next frame the same keys are held, not pressed — and Shift, which is
  // a modifier rather than a key the logic listens for, is not in the list.
  input.endFrame();
  routed = kimia::routeEditorInput(editor, input, 1.0 / 60.0);
  KIMIA_REQUIRE(routed.pressedKeys.empty());
  KIMIA_REQUIRE(routed.heldKeys.size() == 2U);
  KIMIA_REQUIRE(std::find(routed.heldKeys.begin(), routed.heldKeys.end(), "right") !=
                routed.heldKeys.end());
  KIMIA_REQUIRE(std::find(routed.heldKeys.begin(), routed.heldKeys.end(), "up") !=
                routed.heldKeys.end());
  KIMIA_REQUIRE(near(routed.moveX, 1.0));  // a held key keeps moving
  KIMIA_REQUIRE(routed.fine);

  // A controller stick pushing further than the keys wins, which is what a
  // player expects when both are live.
  input.endFrame();
  input.setKeyDown(Key::Right, false);
  input.setKeyDown(Key::Up, false);
  input.setGamepadAxis(GamepadAxis::LeftX, -0.75);
  input.setGamepadAxis(GamepadAxis::LeftY, 0.5);
  routed = kimia::routeEditorInput(editor, input, 1.0 / 60.0);
  KIMIA_REQUIRE(near(routed.moveX, -0.75));
  KIMIA_REQUIRE(near(routed.moveZ, 0.5));

  // An edge is reported once: the key name in pressedKeys and in the logic.
  input.endFrame();
  input.setGamepadAxis(GamepadAxis::LeftX, 0.0);
  input.setGamepadAxis(GamepadAxis::LeftY, 0.0);
  input.setKeyDown(Key::J, true);
  routed = kimia::routeEditorInput(editor, input, 1.0 / 60.0);
  KIMIA_REQUIRE(std::find(routed.pressedKeys.begin(), routed.pressedKeys.end(), "j") !=
                routed.pressedKeys.end());
  input.endFrame();
  routed = kimia::routeEditorInput(editor, input, 1.0 / 60.0);  // still held, not pressed
  KIMIA_REQUIRE(routed.pressedKeys.empty());
}

KIMIA_TEST(view_input_router_handles_ball_control_and_transport) {
  WorldEditor editor = editorWithWorld();
  editor.createWorld(editor.profileAt(0));
  InputState input;

  // Hold C: the deliberate dribble, a level not an edge.
  input.setKeyDown(Key::C, true);
  kimia::routeEditorInput(editor, input, 1.0 / 60.0);
  KIMIA_REQUIRE(editor.dribbleHeld());
  input.endFrame();
  input.setKeyDown(Key::C, false);
  kimia::routeEditorInput(editor, input, 1.0 / 60.0);
  KIMIA_REQUIRE(!editor.dribbleHeld());

  // Curl is a held stick: q is left, e is right, and it survives until the
  // next shot takes it.
  input.endFrame();
  input.setKeyDown(Key::Q, true);
  kimia::routeEditorInput(editor, input, 1.0 / 60.0);
  KIMIA_REQUIRE(near(editor.curl(), -1.0));
  input.endFrame();
  input.setKeyDown(Key::Q, false);
  input.setKeyDown(Key::E, true);
  kimia::routeEditorInput(editor, input, 1.0 / 60.0);
  KIMIA_REQUIRE(near(editor.curl(), 1.0));
  input.endFrame();
  input.setKeyDown(Key::E, false);

  // Transport: Return starts play, Tab pauses it, Backspace steps a frame.
  input.setKeyDown(Key::Return, true);
  kimia::routeEditorInput(editor, input, 1.0 / 60.0);
  KIMIA_REQUIRE(editor.playing());
  input.endFrame();
  input.setKeyDown(Key::Return, false);
  input.setKeyDown(Key::Tab, true);
  kimia::routeEditorInput(editor, input, 1.0 / 60.0);
  KIMIA_REQUIRE(editor.paused());
  input.endFrame();
  input.setKeyDown(Key::Tab, false);
  input.setKeyDown(Key::Backspace, true);
  kimia::routeEditorInput(editor, input, 1.0 / 60.0);
  KIMIA_REQUIRE(editor.paused());  // still paused; one fixed step was taken
}

KIMIA_TEST(view_input_router_hands_the_camera_its_contract) {
  WorldEditor editor = editorWithWorld();
  InputState input;

  // In the editor the arrows orbit the rig, so the distance the user leaves
  // behind is the one to remember.
  KIMIA_REQUIRE(editor.cameraControlled());
  input.setKeyDown(Key::Right, true);
  RoutedInput routed = kimia::routeEditorInput(editor, input, 0.5);
  KIMIA_REQUIRE(routed.camera.handSet);
  KIMIA_REQUIRE(near(routed.camera.yawDelta, 1.1 * 0.5));
  KIMIA_REQUIRE(near(routed.camera.pitchDelta, 0.0));

  // q and e zoom in opposite directions, and they stay separate steps (the
  // order matters at the clamp).
  input.endFrame();
  input.setKeyDown(Key::Right, false);
  input.setKeyDown(Key::Q, true);
  routed = kimia::routeEditorInput(editor, input, 0.5);
  KIMIA_REQUIRE(routed.camera.zoomStepCount == 1);
  KIMIA_REQUIRE(near(routed.camera.zoomSteps[0], 1.0 / 1.2));

  input.endFrame();
  input.setKeyDown(Key::Q, false);
  input.setKeyDown(Key::E, true);
  input.addZoom(-1.0);  // a wheel notch in the same frame
  routed = kimia::routeEditorInput(editor, input, 0.5);
  KIMIA_REQUIRE(routed.camera.zoomStepCount == 2);
  KIMIA_REQUIRE(near(routed.camera.zoomSteps[0], 1.2));
  KIMIA_REQUIRE(near(routed.camera.zoomSteps[1], 1.2));
}

// --- Camera controller -------------------------------------------------------

KIMIA_TEST(view_camera_orbits_zooms_and_remembers_the_hand_set_distance) {
  CameraController camera;
  const f64 startDistance = camera.distance();
  KIMIA_REQUIRE(near(startDistance, kimia::OrbitCamera::kDefaultDistance));

  CameraInput input;
  input.yawDelta = 0.5;
  input.pitchDelta = 0.25;
  input.handSet = true;
  camera.applyInput(input);
  KIMIA_REQUIRE(near(camera.rig().yaw, 0.5));
  KIMIA_REQUIRE(near(camera.rig().pitch, kimia::OrbitCamera::kDefaultPitch + 0.25));
  KIMIA_REQUIRE(near(camera.restingDistance(), startDistance));

  // Zoom, then hand-set: the resting distance follows the rig.
  CameraInput zoom;
  zoom.zoom(2.0);
  zoom.handSet = true;
  camera.applyInput(zoom);
  KIMIA_REQUIRE(near(camera.distance(), startDistance * 2.0));
  KIMIA_REQUIRE(near(camera.restingDistance(), startDistance * 2.0));

  // Zoom steps apply in order and clamp like separate calls would.
  CameraController clamped;
  CameraInput out;
  out.zoom(0.01);  // far below the minimum: lands exactly on it
  out.zoom(1.2);   // ...and then steps back up from the clamp
  clamped.applyInput(out);
  KIMIA_REQUIRE(near(clamped.distance(), kimia::OrbitCamera::kMinDistance * 1.2));

  // Pitch is clamped and reset restores the overview.
  CameraInput tooFar;
  tooFar.pitchDelta = -10.0;
  camera.applyInput(tooFar);
  KIMIA_REQUIRE(near(camera.rig().pitch, kimia::OrbitCamera::kMinPitch));

  CameraInput back;
  back.reset = true;
  camera.applyInput(back);
  KIMIA_REQUIRE(near(camera.rig().yaw, 0.0));
  KIMIA_REQUIRE(near(camera.rig().pitch, kimia::OrbitCamera::kDefaultPitch));
}

KIMIA_TEST(view_camera_view_and_pick_ray_come_from_the_same_rig) {
  WorldEditor editor = editorWithWorld();
  CameraController camera;
  CameraInput input;
  input.handSet = true;
  camera.applyInput(input);
  camera.update(editor, 1.0 / 60.0);

  RenderScene scene;
  camera.applyTo(scene, 640, 480);
  KIMIA_REQUIRE(scene.cameraPosition.x == camera.eye().x);
  KIMIA_REQUIRE(scene.cameraPosition.y == camera.eye().y);
  KIMIA_REQUIRE(scene.cameraPosition.z == camera.eye().z);
  KIMIA_REQUIRE(scene.lightDirection.y < 0.0);  // the key light comes from above

  // A tap on the picture is turned into a world position with the very same
  // matrices the frame was drawn with — otherwise picking would lag the view.
  const kimia::pick::Viewport viewport = camera.viewport(640, 480);
  for (kimia::i32 column = 0; column < 4; ++column) {
    for (kimia::i32 row = 0; row < 4; ++row) {
      KIMIA_REQUIRE(viewport.view.at(column, row) == scene.view.at(column, row));
      KIMIA_REQUIRE(viewport.projection.at(column, row) == scene.projection.at(column, row));
    }
  }
  KIMIA_REQUIRE(viewport.width == 640);
  KIMIA_REQUIRE(viewport.height == 480);
  KIMIA_REQUIRE(near(viewport.eye.x, camera.eye().x));

  // Determinism: the same inputs give bit-identical matrices, which is what
  // makes a replay of a camera move possible at all.
  CameraController again;
  CameraInput same;
  same.handSet = true;
  again.applyInput(same);
  again.update(editor, 1.0 / 60.0);
  RenderScene second;
  again.applyTo(second, 640, 480);
  for (kimia::i32 column = 0; column < 4; ++column) {
    for (kimia::i32 row = 0; row < 4; ++row) {
      KIMIA_REQUIRE(second.view.at(column, row) == scene.view.at(column, row));
      KIMIA_REQUIRE(second.projection.at(column, row) == scene.projection.at(column, row));
    }
  }
}

KIMIA_TEST(view_chase_camera_eases_toward_the_aim) {
  WorldEditor editor = editorWithWorld();
  editor.createWorld(editor.profileAt(0));
  CameraController camera;

  // A world that follows the aim: the rig has to close most of a 90 degree
  // gap in a second, without ever overshooting past the target.
  editor.setAimYaw(kimia::kPi * 0.5);
  const bool follows = editor.cameraFollowsAim();
  for (int frame = 0; frame < 60; ++frame) camera.update(editor, 1.0 / 60.0);
  if (follows) {
    KIMIA_REQUIRE(near(camera.yaw(), kimia::kPi * 0.5, 1e-3));
  } else {
    KIMIA_REQUIRE(near(camera.yaw(), 0.0, 1e-9));  // a broadcast camera stays put
  }
  // The target always tracks the world's own answer.
  const Vec3 wanted = editor.cameraTarget();
  KIMIA_REQUIRE(near(camera.target().x, wanted.x));
  KIMIA_REQUIRE(near(camera.target().y, wanted.y));
  KIMIA_REQUIRE(near(camera.target().z, wanted.z));
}

// --- Scene builder -----------------------------------------------------------

KIMIA_TEST(view_scene_builder_uses_the_asset_manager_and_never_reloads) {
  AssetManager assets;
  assets.setProjectRoot(KIMIA_ASSET_DIR);
  RenderSceneBuilder builder(assets);

  WorldEditor editor = editorWithWorld();
  editor.setImportDirectory(KIMIA_ASSET_DIR);
  std::string error;
  const std::string placed = editor.importModel("crate.obj", 1.0, error);
  KIMIA_REQUIRE(!placed.empty());
  KIMIA_REQUIRE(error.empty());

  RenderScene scene;
  builder.build(editor, scene);
  KIMIA_REQUIRE(!scene.objects.empty());
  KIMIA_REQUIRE(builder.report().objects == scene.objects.size());
  // The ground plus the crate.
  KIMIA_REQUIRE(scene.objects.size() >= 2U);
  // Exactly one trip to the disk for the model and its material table, and it
  // is the manager's parse rather than a second cache of the same file.
  KIMIA_REQUIRE(assets.stats().loads >= 1U);
  const kimia::u64 loadsAfterFirstBuild = assets.stats().loads;

  // A hundred frames of the same world: the draw list is rebuilt, but not one
  // file is opened again, and not one mesh is copied. This is the acceptance
  // criterion of phase 2 — the frame loop has no loading in its hot path.
  for (int frame = 0; frame < 100; ++frame) {
    RenderScene frameScene;
    builder.build(editor, frameScene);
    KIMIA_REQUIRE(frameScene.objects.size() == scene.objects.size());
    KIMIA_REQUIRE(meshPointers(frameScene) == meshPointers(scene));
  }
  KIMIA_REQUIRE(assets.stats().loads == loadsAfterFirstBuild);
  KIMIA_REQUIRE(assets.stats().hits > 0U);
  KIMIA_REQUIRE(assets.lastError().empty());
  KIMIA_REQUIRE(builder.failedAssets().empty());  // nothing failed: nothing to explain

  // The crate's geometry comes from the manager's own parse of the file: the
  // merged mesh, or one of the per-material sub-meshes of the same asset.
  const kimia::assets::MeshAsset* asset = assets.meshAsset("crate.obj");
  KIMIA_REQUIRE(asset != nullptr);
  bool foundModelMesh = false;
  for (const kimia::RenderObject& object : scene.objects) {
    if (object.mesh == &asset->mesh) foundModelMesh = true;
    for (const kimia::MeshData& sub : asset->subMeshes) {
      if (object.mesh == &sub) foundModelMesh = true;
    }
  }
  KIMIA_REQUIRE(foundModelMesh);
}

KIMIA_TEST(view_scene_builder_reports_a_model_it_cannot_read) {
  AssetManager assets;
  assets.setProjectRoot(KIMIA_ASSET_DIR);
  RenderSceneBuilder builder(assets);

  // A world whose assets did not travel with it. This is the realistic path:
  // a .kimia file saved on another machine, loaded here with its model and its
  // picture missing. (The editor itself refuses to attach a file it cannot
  // read, so this has to arrive from disk.)
  kimia::Scene worldScene;
  const kimia::EntityHandle ground = worldScene.create("Ground");
  kimia::EntityData* groundData = worldScene.get(ground);
  groundData->mesh = kimia::MeshKind::plane;
  groundData->texture = "textures/ghost.png";
  const kimia::EntityHandle ghost = worldScene.create("Model_Ghost");
  kimia::EntityData* ghostData = worldScene.get(ghost);
  ghostData->meshFile = "models/ghost.fbx";
  KIMIA_REQUIRE(kimia::SceneIO::saveToFile(worldScene, tmpPath("view_missing.kimia")));

  WorldEditor editor;
  std::string error;
  KIMIA_REQUIRE(editor.loadWorld(tmpPath("view_missing.kimia"), error));

  RenderScene scene;
  builder.build(editor, scene);
  // The model entity is skipped, exactly as it always was, and the frame still
  // draws: an unreadable model is a smaller scene, not a crash and not a
  // stutter. The ground is there, without its picture.
  KIMIA_REQUIRE(scene.objects.size() == 1U);
  KIMIA_REQUIRE(scene.objects[0].texture == nullptr);
  KIMIA_REQUIRE(builder.report().missingAssets == 2U);
  const std::vector<std::string> missing = assets.missingAssets();
  KIMIA_REQUIRE(missing.size() == 2U);
  KIMIA_REQUIRE(missing[0] == "models/ghost.fbx");    // sorted: the model first
  KIMIA_REQUIRE(missing[1] == "textures/ghost.png");

  // The skip is explainable: which file, and what the manager said about it.
  const std::map<std::string, std::string> failed = builder.failedAssets();
  KIMIA_REQUIRE(failed.size() == 1U);
  KIMIA_REQUIRE(failed.count("models/ghost.fbx") == 1U);
  KIMIA_REQUIRE(failed.at("models/ghost.fbx").find("not found") != std::string::npos);

  // Ten more frames: still exactly two failures, still one object, same
  // pointers. A world whose assets moved does not become a world that stutters.
  const std::vector<const kimia::MeshData*> before = meshPointers(scene);
  for (int frame = 0; frame < 10; ++frame) {
    builder.build(editor, scene);
    KIMIA_REQUIRE(scene.objects.size() == 1U);
    KIMIA_REQUIRE(meshPointers(scene) == before);
  }
  KIMIA_REQUIRE(assets.stats().failures == 2U);
}

KIMIA_TEST(view_scene_builder_draws_the_ghost_the_markers_and_the_ball) {
  AssetManager assets;
  assets.setProjectRoot(KIMIA_ASSET_DIR);
  RenderSceneBuilder builder(assets);

  WorldEditor editor = editorWithWorld();
  editor.createWorld(editor.profileAt(0));
  RenderScene scene;
  builder.build(editor, scene);
  const kimia::usize emptyStage = scene.objects.size();
  KIMIA_REQUIRE(emptyStage >= 1U);  // the ground of an empty project

  // The placement ghost appears while placing, one shape per object kind: it
  // is the only difference between the empty stage and this frame.
  editor.choose(0);  // catalog
  editor.choose(2);  // block
  editor.choose(1);  // size
  KIMIA_REQUIRE(editor.placing());
  editor.setGhostPosition(Vec3{1.0, 0.0, -1.0});
  RenderScene ghostScene;
  builder.build(editor, ghostScene);
  KIMIA_REQUIRE(ghostScene.objects.size() == emptyStage + 1U);

  // Place it, leave the placement screen and work on it: the eight corner
  // markers of the selected object show up on top of the object itself.
  editor.choose(0);  // place
  editor.choose(1);  // back to the builder
  editor.choose(1);  // builder -> manage (the hierarchy list)
  KIMIA_REQUIRE(editor.managedCount() == 1U);
  editor.choose(0);  // pick the placed block -> inspector (now something IS selected)
  KIMIA_REQUIRE(editor.managedName() == "Block_1");
  KIMIA_REQUIRE(editor.selectedEntity() != nullptr);
  RenderScene manageScene;
  builder.build(editor, manageScene);
  KIMIA_REQUIRE(manageScene.objects.size() == emptyStage + 1U + 8U);

  // The ball is drawn as soon as it exists, and follows the physics body
  // during play. A fresh editor: this part must not depend on the menu trail
  // above, and the builder is happy to serve a second world.
  WorldEditor ballEditor = editorWithWorld();
  ballEditor.createWorld(ballEditor.profileAt(0));
  ballEditor.choose(0);  // catalog
  ballEditor.choose(1);  // ball
  ballEditor.choose(0);  // the football
  ballEditor.setGhostPosition(Vec3{0.0, 0.0, 2.0});
  ballEditor.choose(0);  // place
  KIMIA_REQUIRE(ballEditor.world().scene.find("Ball") != 0);
  ballEditor.setBallPosition(Vec3{1.5, 0.2, -0.5});
  RenderScene ballScene;
  builder.build(ballEditor, ballScene);
  const Vec3 ball = ballEditor.ballPosition();
  bool ballDrawn = false;
  for (const kimia::RenderObject& object : ballScene.objects) {
    // The ball is the sphere primitive placed at the ball's own position.
    // Mat4 stores its translation in column 3 (see Mat4::translation).
    if (object.mesh == &builder.sphereMesh() && near(object.model.at(3, 0), ball.x, 1e-6) &&
        near(object.model.at(3, 2), ball.z, 1e-6)) {
      ballDrawn = true;
    }
  }
  KIMIA_REQUIRE(ballDrawn);
}
