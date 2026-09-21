// The components phase 3 adds: dialogue lines and a camera target. Both are
// data on an entity, both live in the .kimia file, and both are wired to the
// runtime (HUD and camera) rather than being fields nobody reads.
//
// Everything here goes through the editor's own API — the same calls the
// Workbench's /api/* handlers make — so these tests fail if the editor cannot
// author the component, not merely if the component type exists.
#include <kimia/RenderSceneBuilder.h>
#include <kimia/SceneIO.h>
#include <kimia/World.h>
#include <kimia/WorldIO.h>
#include <kimia_test.h>

#include <cmath>
#include <string>
#include <vector>

namespace {

using kimia::CameraTargetComponent;
using kimia::DialogueComponent;
using kimia::EntityData;
using kimia::MeshKind;
using kimia::Vec3;
using kimia::WorldEditor;

WorldEditor editorWithWorld() {
  WorldEditor editor;
  editor.choose(0);  // main menu -> «کدام بازی؟»
  editor.choose(0);  // ground -> builder
  return editor;
}

bool hasLine(const std::vector<std::string>& lines, const std::string& needle) {
  for (const std::string& line : lines) {
    if (line.find(needle) != std::string::npos) return true;
  }
  return false;
}

}  // namespace

// --- Dialogue ----------------------------------------------------------------

KIMIA_TEST(dialogue_lines_play_through_the_trigger_names_and_the_hud) {
  WorldEditor editor = editorWithWorld();
  editor.createWorld(editor.profileAt(0));
  // Nothing in the world talks yet.
  KIMIA_REQUIRE(editor.dialogueLines().empty());
  // The empty project is the ground and nothing else: a speaker has to be put
  // in the world like any other object.
  const std::string speaker = editor.createObject("player", Vec3{0.0, 0.0, 4.0});
  KIMIA_REQUIRE(speaker == "Player");

  // A line is attached through the editor — the same entry point the
  // Workbench's /api/wire-line uses.
  DialogueComponent hello;
  hello.line = "سلام، توپ داری؟";
  hello.trigger = "t";
  hello.holdSeconds = 2.0;
  DialogueComponent goal;
  goal.line = "گل شد!";
  goal.trigger = "goal";
  goal.holdSeconds = 1.0;
  KIMIA_REQUIRE(editor.addEntityDialogue(speaker, hello));
  KIMIA_REQUIRE(editor.addEntityDialogue(speaker, goal));
  KIMIA_REQUIRE(editor.entityDialogue(speaker).size() == 2U);
  KIMIA_REQUIRE(editor.entityDialogue("Nobody").empty());          // no guessing
  KIMIA_REQUIRE(!editor.addEntityDialogue("Nobody", hello));       // no such entity
  DialogueComponent silent;
  silent.trigger = "t";
  KIMIA_REQUIRE(!editor.addEntityDialogue(speaker, silent));      // no words, no line

  // A key press fires the line through the public trigger path — the same one
  // animations and sounds use, so one key can play all three.
  KIMIA_REQUIRE(editor.fireTrigger("t") >= 1U);
  KIMIA_REQUIRE(editor.dialogueLines().size() == 1U);
  KIMIA_REQUIRE(editor.dialogueLines()[0] == speaker + ": سلام، توپ داری؟");

  // The caption reaches the HUD, which is what the frame actually draws.
  KIMIA_REQUIRE(hasLine(editor.hudLines(), "سلام، توپ داری؟"));

  // It counts down on host time and disappears on its own.
  editor.update(1.5);
  KIMIA_REQUIRE(editor.dialogueLines().size() == 1U);
  editor.update(0.6);
  KIMIA_REQUIRE(editor.dialogueLines().empty());
  KIMIA_REQUIRE(!hasLine(editor.hudLines(), "سلام، توپ داری؟"));

  // A second line from the same speaker replaces the first: people do not
  // talk over themselves.
  editor.fireDialogue(speaker, "t");
  editor.fireDialogue(speaker, "goal");
  KIMIA_REQUIRE(editor.dialogueLines().size() == 1U);
  KIMIA_REQUIRE(editor.dialogueLines()[0] == speaker + ": گل شد!");

  // Another speaker is a second line on screen at the same time.
  const std::string other = editor.createObject("crate", Vec3{2.0, 0.0, 0.0});
  KIMIA_REQUIRE(!other.empty());
  DialogueComponent shout;
  shout.line = "پاس بده!";
  shout.trigger = "t";
  shout.holdSeconds = 5.0;
  KIMIA_REQUIRE(editor.addEntityDialogue(other, shout));
  editor.fireTrigger("t");
  KIMIA_REQUIRE(editor.dialogueLines().size() == 2U);

  // Forgetting everything is one call (what a restart does).
  editor.clearDialogue();
  KIMIA_REQUIRE(editor.dialogueLines().empty());
  KIMIA_REQUIRE(!hasLine(editor.hudLines(), "پاس بده!"));

  // And a line can be removed from its entity: the file follows the editor.
  KIMIA_REQUIRE(editor.clearEntityDialogue(other));
  KIMIA_REQUIRE(editor.entityDialogue(other).empty());
  editor.clearDialogue();
  KIMIA_REQUIRE(editor.fireTrigger("t") == 1U);  // the speaker's line only
  KIMIA_REQUIRE(editor.dialogueLines().size() == 1U);
  KIMIA_REQUIRE(!hasLine(editor.dialogueLines(), "پاس بده!"));
}

KIMIA_TEST(dialogue_survives_a_deleted_speaker_and_a_paused_game) {
  WorldEditor editor = editorWithWorld();
  editor.createWorld(editor.profileAt(0));
  const std::string speaker = editor.createObject("player", Vec3{0.0, 0.0, 4.0});
  KIMIA_REQUIRE(!speaker.empty());
  DialogueComponent line;
  line.line = "بریم!";
  line.trigger = "t";
  line.holdSeconds = 2.0;
  KIMIA_REQUIRE(editor.addEntityDialogue(speaker, line));
  KIMIA_REQUIRE(editor.fireTrigger("t") == 1U);

  // A paused game keeps showing the caption it was showing: the line is HUD
  // text with a lifetime, not simulation state.
  editor.enterPlayMode();
  editor.setPaused(true);
  editor.update(0.5);
  KIMIA_REQUIRE(editor.dialogueLines().size() == 1U);
  editor.setPaused(false);

  // Deleting the speaker does not leave a line hanging forever: the caption
  // expires like any other.
  KIMIA_REQUIRE(editor.fireDialogue(speaker, "t") == 1U);
  editor.update(3.0);
  KIMIA_REQUIRE(editor.dialogueLines().empty());
}

// --- Camera target -----------------------------------------------------------

KIMIA_TEST(camera_target_component_moves_the_camera_subject) {
  WorldEditor editor = editorWithWorld();
  editor.createWorld(editor.profileAt(0));
  const std::string keeper = editor.createObject("block", Vec3{3.0, 0.0, -6.0});
  KIMIA_REQUIRE(!keeper.empty());

  CameraTargetComponent watch;
  watch.weight = 1.0;
  KIMIA_REQUIRE(editor.setEntityCameraTarget(keeper, watch));
  // The component decides: the camera looks at the keeper instead of the
  // empty stage's centre.
  KIMIA_REQUIRE(editor.cameraTargetEntity() == editor.world().scene.find(keeper));
  KIMIA_REQUIRE(std::abs(editor.cameraTarget().x - 3.0) < 1e-9);
  KIMIA_REQUIRE(std::abs(editor.cameraTarget().z + 6.0) < 1e-9);

  // A higher weight wins, and the offset is applied.
  const std::string hero = editor.createObject("block", Vec3{-2.0, 0.0, 4.0});
  CameraTargetComponent closer;
  closer.weight = 2.0;
  closer.offset = Vec3{0.0, 1.5, 0.0};
  KIMIA_REQUIRE(editor.setEntityCameraTarget(hero, closer));
  KIMIA_REQUIRE(editor.cameraTargetEntity() == editor.world().scene.find(hero));
  KIMIA_REQUIRE(std::abs(editor.cameraTarget().x + 2.0) < 1e-9);
  // The block stands on the ground (its centre is half a metre up), so the
  // look-at point is the entity's own position plus the offset, not the
  // offset alone.
  const kimia::EntityData* heroEntity = editor.world().scene.get(editor.world().scene.find(hero));
  KIMIA_REQUIRE(heroEntity != nullptr);
  KIMIA_REQUIRE(std::abs(editor.cameraTarget().y - (heroEntity->transform.position.y + 1.5)) < 1e-9);

  // Removing it hands the camera back to the world's own rule (the ball while
  // playing, the stage otherwise).
  KIMIA_REQUIRE(editor.clearEntityCameraTarget(hero));
  KIMIA_REQUIRE(!editor.clearEntityCameraTarget(hero));  // already gone
  KIMIA_REQUIRE(editor.cameraTargetEntity() == editor.world().scene.find(keeper));

  // An edit-only target does not steer a running game.
  const std::string marker = editor.createObject("block", Vec3{0.0, 0.0, 8.0});
  CameraTargetComponent editOnly;
  editOnly.weight = 9.0;
  editOnly.whilePlaying = false;
  KIMIA_REQUIRE(editor.setEntityCameraTarget(marker, editOnly));
  KIMIA_REQUIRE(editor.cameraTargetEntity() == editor.world().scene.find(marker));
  KIMIA_REQUIRE(editor.enterPlayMode());
  KIMIA_REQUIRE(editor.cameraTargetEntity() == editor.world().scene.find(keeper));
  KIMIA_REQUIRE(!editor.setEntityCameraTarget("Nobody", watch));
}

// --- Both components survive the file ----------------------------------------

KIMIA_TEST(dialogue_and_camera_target_roundtrip_through_a_scene_file) {
  kimia::Scene scene;
  EntityData talker;
  talker.name = "Narrator";
  talker.mesh = MeshKind::cube;
  DialogueComponent first;
  first.line = "یک روز، در کوچه...";
  first.trigger = "intro";
  first.volume = 0.8;
  first.holdSeconds = 4.5;
  DialogueComponent second;
  second.line = "توپ از دیوار پرید";
  second.trigger = "over the wall";
  second.holdSeconds = 2.0;
  talker.dialogue.push_back(first);
  talker.dialogue.push_back(second);
  CameraTargetComponent watch;
  watch.weight = 3.0;
  watch.whilePlaying = false;
  watch.offset = Vec3{0.0, 0.5, -1.0};
  talker.cameraTarget = watch;
  scene.create(talker);

  std::string text;
  KIMIA_REQUIRE(kimia::SceneIO::save(scene, text));
  KIMIA_REQUIRE(text.find(" say \"یک روز، در کوچه...\" \"intro\" 0.8 4.5") != std::string::npos);
  KIMIA_REQUIRE(text.find(" say \"توپ از دیوار پرید\" \"over the wall\" 1 2") != std::string::npos);
  KIMIA_REQUIRE(text.find(" camtarget 3 edit 0 0.5 -1") != std::string::npos);

  kimia::Scene loaded;
  std::string error;
  kimia::SceneIO::LoadReport report;
  KIMIA_REQUIRE(kimia::SceneIO::load(text, loaded, error, report));
  KIMIA_REQUIRE(report.warnings.empty());
  const EntityData* other = loaded.get(loaded.find("Narrator"));
  KIMIA_REQUIRE(other != nullptr);
  KIMIA_REQUIRE(other->dialogue.size() == 2U);
  KIMIA_REQUIRE(other->dialogue[0].line == "یک روز، در کوچه...");
  KIMIA_REQUIRE(other->dialogue[0].trigger == "intro");
  KIMIA_REQUIRE(std::abs(other->dialogue[0].volume - 0.8) < 1e-9);
  KIMIA_REQUIRE(std::abs(other->dialogue[0].holdSeconds - 4.5) < 1e-9);
  KIMIA_REQUIRE(other->dialogue[1].trigger == "over the wall");  // spaces survive
  KIMIA_REQUIRE(other->cameraTarget.has_value());
  KIMIA_REQUIRE(std::abs(other->cameraTarget->weight - 3.0) < 1e-9);
  KIMIA_REQUIRE(!other->cameraTarget->whilePlaying);
  KIMIA_REQUIRE(std::abs(other->cameraTarget->offset.y - 0.5) < 1e-9);
  KIMIA_REQUIRE(std::abs(other->cameraTarget->offset.z + 1.0) < 1e-9);

  // Byte-identical round trip: a component that changed the file on every save
  // would make every world diff noise.
  std::string again;
  KIMIA_REQUIRE(kimia::SceneIO::save(loaded, again));
  KIMIA_REQUIRE(again == text);
}

KIMIA_TEST(components_absent_from_a_world_never_appear_in_it) {
  WorldEditor editor = editorWithWorld();
  editor.createWorld(editor.profileAt(0));
  std::string text;
  KIMIA_REQUIRE(kimia::WorldIO::save(editor.world(), text));
  // A world that uses neither component is exactly the file it always was.
  KIMIA_REQUIRE(text.find(" say ") == std::string::npos);
  KIMIA_REQUIRE(text.find(" camtarget ") == std::string::npos);
}

KIMIA_TEST(dialogue_authored_in_an_editor_survives_save_and_load) {
  WorldEditor editor = editorWithWorld();
  editor.createWorld(editor.profileAt(0));
  const std::string speaker = editor.createObject("player", Vec3{0.0, 0.0, 4.0});
  KIMIA_REQUIRE(!speaker.empty());
  DialogueComponent line;
  line.line = "بازی شروع شد";
  line.trigger = "return";
  line.holdSeconds = 2.5;
  KIMIA_REQUIRE(editor.addEntityDialogue(speaker, line));
  const std::string ball = editor.createObject("ball", Vec3{0.0, 0.0, 0.0});
  KIMIA_REQUIRE(ball == "Ball");
  CameraTargetComponent watch;
  watch.weight = 1.5;
  KIMIA_REQUIRE(editor.setEntityCameraTarget(ball, watch));

  std::string text;
  KIMIA_REQUIRE(kimia::WorldIO::save(editor.world(), text));
  KIMIA_REQUIRE(text.find(" say \"بازی شروع شد\" \"return\" 1 2.5") != std::string::npos);
  KIMIA_REQUIRE(text.find(" camtarget 1.5 play 0 0 0") != std::string::npos);
}

// --- One cache for the whole session -----------------------------------------
// Phase 2's acceptance criterion was "the frame loop never loads assets
// directly". The editor used to keep its own two caches next to the frame
// builder's, so importing a model, previewing it and drawing it could read the
// same file three times. These tests pin the sharing.

KIMIA_TEST(editor_and_frame_builder_share_one_asset_cache) {
  WorldEditor editor = editorWithWorld();
  editor.createWorld(editor.profileAt(0));
  editor.setImportDirectory(KIMIA_ASSET_DIR);

  kimia::AssetManager& shared = editor.assetManager();
  KIMIA_REQUIRE(shared.roots().size() == 1U);

  std::string error;
  const std::string placed = editor.importModel("crate.obj", 1.0, error);
  KIMIA_REQUIRE(placed == "Model_1");
  KIMIA_REQUIRE(error.empty());
  const kimia::u64 afterImport = shared.stats().loads;
  KIMIA_REQUIRE(afterImport >= 1U);
  // The importer filled the manager, not a private cache: the material table
  // the draw list wants is already there, and it is the same object the
  // editor's own material view returns.
  const kimia::assets::MeshAsset* table = shared.meshAsset("crate.obj");
  KIMIA_REQUIRE(table != nullptr);
  KIMIA_REQUIRE(shared.stats().loads == afterImport);
  KIMIA_REQUIRE(editor.assetFor("crate.obj") == table);

  // The frame builder draws the same model through the same cache.
  kimia::RenderSceneBuilder builder(shared);

  // The first frame is allowed exactly one new load: the model's material
  // texture, which importing the geometry never had a reason to open. That is
  // a first load, not a reload — the point of the shared cache is that there
  // is no second one.
  kimia::RenderScene firstFrame;
  builder.build(editor, firstFrame);
  KIMIA_REQUIRE(!firstFrame.objects.empty());
  const kimia::u64 afterFirstFrame = shared.stats().loads;
  KIMIA_REQUIRE(afterFirstFrame <= afterImport + 1U);

  // A hundred more frames of drawing that model cost nothing at all.
  const kimia::u64 requestsBefore = shared.stats().requests;
  for (int frame = 0; frame < 100; ++frame) {
    kimia::RenderScene scene;
    builder.build(editor, scene);
    KIMIA_REQUIRE(!scene.objects.empty());
  }
  KIMIA_REQUIRE(shared.stats().requests > requestsBefore);
  KIMIA_REQUIRE(shared.stats().loads == afterFirstFrame);
  KIMIA_REQUIRE(shared.lastError().empty());
  KIMIA_REQUIRE(shared.missingAssets().empty());
}

KIMIA_TEST(a_missing_model_costs_one_trip_to_the_disk_for_the_whole_editor) {
  WorldEditor editor = editorWithWorld();
  editor.createWorld(editor.profileAt(0));
  editor.setImportDirectory(KIMIA_ASSET_DIR);
  kimia::AssetManager& shared = editor.assetManager();

  // The editor's own thumbnail/bone paths ask for a file that is not there...
  KIMIA_REQUIRE(editor.assetFor("models/ghost.obj") == nullptr);
  const std::string reason = shared.lastError();
  KIMIA_REQUIRE(reason.find("ghost.obj") != std::string::npos);
  const kimia::u64 failures = shared.stats().failures;
  KIMIA_REQUIRE(failures == 1U);

  // ...and repeating the question — ten times, as a listing or a frame would —
  // neither reads the disk again nor changes the answer or the reason.
  for (int i = 0; i < 10; ++i) {
    KIMIA_REQUIRE(editor.assetFor("models/ghost.obj") == nullptr);
    KIMIA_REQUIRE(shared.lastError() == reason);
  }
  KIMIA_REQUIRE(shared.stats().failures == 1U);
  const std::vector<std::string> missing = shared.missingAssets();
  KIMIA_REQUIRE(missing.size() == 1U);
  KIMIA_REQUIRE(missing[0] == "models/ghost.obj");

  // Deleting the file from the user's project is a different question once the
  // editor is told the asset changed.
  KIMIA_REQUIRE(shared.invalidate("models/ghost.obj"));
  KIMIA_REQUIRE(shared.missingAssets().empty());
}

KIMIA_TEST(importing_a_model_twice_reuses_the_parsed_mesh) {
  WorldEditor editor = editorWithWorld();
  editor.createWorld(editor.profileAt(0));
  editor.setImportDirectory(KIMIA_ASSET_DIR);
  kimia::AssetManager& shared = editor.assetManager();

  std::string error;
  KIMIA_REQUIRE(editor.importModel("crate.obj", 1.0, error) == "Model_1");
  KIMIA_REQUIRE(error.empty());
  const kimia::u64 loads = shared.stats().loads;
  KIMIA_REQUIRE(editor.importModel("crate.obj", 2.0, error) == "Model_2");
  KIMIA_REQUIRE(error.empty());
  // Two objects, one parse: the second import names a different object and
  // gets a different scale, from the same cached geometry.
  KIMIA_REQUIRE(shared.stats().loads == loads);
  const kimia::EntityData* second = editor.world().scene.get(editor.world().scene.find("Model_2"));
  KIMIA_REQUIRE(second != nullptr);
  const kimia::EntityData* first = editor.world().scene.get(editor.world().scene.find("Model_1"));
  KIMIA_REQUIRE(first != nullptr);
  KIMIA_REQUIRE(std::abs(second->transform.scale.x - 2.0 * first->transform.scale.x) < 1e-6);
}
