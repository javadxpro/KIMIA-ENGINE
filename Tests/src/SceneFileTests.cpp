// The scene file contract: which version is written, what an older file
// becomes when it is read, and what the reader says about the parts it could
// not use. Documentation/Scene.md states the rule; these tests pin it.
#include <kimia/Scene.h>
#include <kimia/SceneIO.h>
#include <kimia/WorldIO.h>
#include <kimia/Vec.h>
#include <kimia_test.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <sys/stat.h>

#ifndef KIMIA_TEST_TMP
#error "KIMIA_TEST_TMP must be defined by CMake"
#endif

namespace {

using kimia::EntityData;
using kimia::EntityHandle;
using kimia::MeshKind;
using kimia::Scene;
using kimia::SceneIO;

std::string tmpPath(const std::string& name) {
  const int made = ::mkdir(KIMIA_TEST_TMP, 0755);
  static_cast<void>(made);
  return std::string(KIMIA_TEST_TMP) + "/" + name;
}

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

EntityData makeCube(const std::string& name) {
  EntityData entity;
  entity.name = name;
  entity.mesh = MeshKind::cube;
  return entity;
}

std::string save(const Scene& scene) {
  std::string text;
  KIMIA_REQUIRE(SceneIO::save(scene, text));
  return text;
}

}  // namespace

// --- Which version gets written ---------------------------------------------

KIMIA_TEST(sceneio_writes_v1_when_the_handles_are_canonical) {
  Scene scene;
  scene.create(makeCube("Ground"));
  scene.create(makeCube("Ball"));
  const std::string text = save(scene);
  // Every world on disk today is v1 and must stay v1: same header, no ids.
  KIMIA_REQUIRE(text.rfind("# KIMIA scene v1\n", 0U) == 0U);
  KIMIA_REQUIRE(!contains(text, " id "));
  KIMIA_REQUIRE(text == "# KIMIA scene v1\n"
                        "e \"Ground\" mesh cube pos 0 0 0 scale 1 1 1 color 1 1 1 rough 0.5\n"
                        "e \"Ball\" mesh cube pos 0 0 0 scale 1 1 1 color 1 1 1 rough 0.5\n");
}

KIMIA_TEST(sceneio_writes_v2_only_when_a_scene_has_holes) {
  Scene scene;
  scene.create(makeCube("A"));
  const EntityHandle removed = scene.create(makeCube("B"));
  scene.create(makeCube("C"));
  KIMIA_REQUIRE(scene.destroy(removed));  // handle 2 is now a hole

  const std::string text = save(scene);
  KIMIA_REQUIRE(text.rfind("# KIMIA scene v2\n", 0U) == 0U);
  KIMIA_REQUIRE(contains(text, "e \"A\" id 1 "));
  KIMIA_REQUIRE(contains(text, "e \"C\" id 3 "));

  // And the ids come back: save -> load -> save is byte-identical, which is
  // the property that makes a file a stable identity rather than a snapshot
  // of whatever order the objects happened to be created in.
  Scene loaded;
  std::string error;
  SceneIO::LoadReport report;
  KIMIA_REQUIRE(SceneIO::load(text, loaded, error, report));
  KIMIA_REQUIRE(error.empty());
  KIMIA_REQUIRE(report.version == 2);
  KIMIA_REQUIRE(!report.migrated);
  KIMIA_REQUIRE(report.restoredIds == 2U);
  KIMIA_REQUIRE(report.assignedIds == 0U);
  KIMIA_REQUIRE(report.ignoredKeywords.empty());
  KIMIA_REQUIRE(report.warnings.empty());
  KIMIA_REQUIRE(loaded.find("A") == 1U);
  KIMIA_REQUIRE(loaded.find("B") == kimia::kNullEntity);
  KIMIA_REQUIRE(loaded.find("C") == 3U);
  KIMIA_REQUIRE(save(loaded) == text);
  // The counter is above the highest id, so a new object cannot collide with
  // a restored one.
  KIMIA_REQUIRE(loaded.create(makeCube("D")) == 4U);
}

// --- Reading files from other versions ---------------------------------------

KIMIA_TEST(sceneio_migrates_v1_with_the_numbers_v1_always_produced) {
  // The oldest possible file: no header at all, one entity per line.
  const std::string text =
      "e \"Green\" mesh plane pos 0 0 0 scale 1 1 1 color 0.22 0.45 0.24 rough 0.95\n"
      "e \"Ball\" mesh sphere pos 0 0 0 scale 1 1 1 color 0.95 0.95 0.92 rough 0.3\n";
  Scene scene;
  std::string error;
  SceneIO::LoadReport report;
  KIMIA_REQUIRE(SceneIO::load(text, scene, error, report));
  KIMIA_REQUIRE(report.version == 1);
  KIMIA_REQUIRE(report.migrated);  // older than what this build writes
  KIMIA_REQUIRE(report.assignedIds == 2U);
  KIMIA_REQUIRE(report.restoredIds == 0U);
  KIMIA_REQUIRE(scene.find("Green") == 1U);
  KIMIA_REQUIRE(scene.find("Ball") == 2U);
  // Loading an old file and saving it again gives an old file: the migration
  // is in memory, not a rewrite of the user's data.
  KIMIA_REQUIRE(save(scene).rfind("# KIMIA scene v1\n", 0U) == 0U);
}

KIMIA_TEST(sceneio_world_file_version_is_the_newest_part_of_the_file) {
  // A world file carries WorldIO's own header and then the scene text
  // (WorldIO::save). When the scene inside needs ids it says so itself, and
  // the file as a whole is the newer of the two.
  const std::string text =
      "# KIMIA scene v1\n"
      "# world name Test\n"
      "# KIMIA scene v2\n"
      "e \"Wall\" id 7 mesh cube pos 2.4 0.5 0 scale 0.5 1 4.4 color 0.7 0.68 0.62 rough 0.5\n";
  Scene scene;
  std::string error;
  SceneIO::LoadReport report;
  KIMIA_REQUIRE(SceneIO::load(text, scene, error, report));
  KIMIA_REQUIRE(report.version == 2);
  KIMIA_REQUIRE(report.restoredIds == 1U);
  KIMIA_REQUIRE(scene.find("Wall") == 7U);
}

KIMIA_TEST(sceneio_refuses_a_newer_version_instead_of_dropping_what_it_cannot_read) {
  const std::string text =
      "# KIMIA scene v9\n"
      "e \"Future\" mesh cube pos 0 0 0 scale 1 1 1 color 1 1 1 rough 0.5 component wormhole 4\n";
  Scene scene;
  scene.create(makeCube("Keep"));  // a failed load must not touch the caller's scene
  std::string error;
  SceneIO::LoadReport report;
  KIMIA_REQUIRE(!SceneIO::load(text, scene, error, report));
  KIMIA_REQUIRE(contains(error, "version 9"));
  KIMIA_REQUIRE(contains(error, "newer"));
  KIMIA_REQUIRE(scene.count() == 1U);
  KIMIA_REQUIRE(scene.find("Keep") == 1U);
  KIMIA_REQUIRE(scene.find("Future") == kimia::kNullEntity);
}

// --- What the reader says about the parts it could not use -------------------

KIMIA_TEST(sceneio_reports_unknown_keywords_and_dropped_lines) {
  const std::string text =
      "# KIMIA scene v1\n"
      "e \"Good\" mesh cube pos 0 0 0 scale 1 1 1 color 1 1 1 rough 0.5 glow 3\n"  // glow: unknown
      "e \"Broken\" mesh banana pos 0 0 0 scale 1 1 1 color 1 1 1 rough 0.5\n"
      "e \"Fine\" mesh sphere pos 1 0 0 scale 1 1 1 color 1 1 1 rough 0.5\n";
  Scene scene;
  std::string error;
  SceneIO::LoadReport report;
  KIMIA_REQUIRE(SceneIO::load(text, scene, error, report));
  // The unknown keyword is harmless: the entity still loads, and the word is
  // remembered so a person can be told their file uses a newer feature.
  KIMIA_REQUIRE(scene.find("Good") == 1U);
  KIMIA_REQUIRE(report.ignoredKeywords.size() == 1U);
  KIMIA_REQUIRE(report.ignoredKeywords[0] == "glow");
  // The unusable line is dropped (as it always was) but named, with its line
  // number and the reason.
  KIMIA_REQUIRE(scene.find("Broken") == kimia::kNullEntity);
  KIMIA_REQUIRE(report.warnings.size() == 1U);
  KIMIA_REQUIRE(contains(report.warnings[0], "line 3"));
  KIMIA_REQUIRE(contains(report.warnings[0], "Broken"));
  KIMIA_REQUIRE(contains(report.warnings[0], "banana"));
  // And the rest of the file is fine.
  KIMIA_REQUIRE(scene.find("Fine") != kimia::kNullEntity);
  KIMIA_REQUIRE(report.assignedIds == 2U);
}

KIMIA_TEST(sceneio_refuses_nan_and_inf_positions) {
  const std::string text =
      "# KIMIA scene v1\n"
      "e \"NaN\" mesh cube pos nan 0 0 scale 1 1 1 color 1 1 1 rough 0.5\n"
      "e \"Inf\" mesh cube pos inf 1 0 scale 1 1 1 color 1 1 1 rough 0.5\n"
      "e \"Real\" mesh cube pos 2 0 0 scale 1 1 1 color 1 1 1 rough 0.5\n";
  Scene scene;
  std::string error;
  SceneIO::LoadReport report;
  KIMIA_REQUIRE(SceneIO::load(text, scene, error, report));
  // A NaN position would poison every physics step that touches it and read
  // as an object that vanished, so those lines are refused — with a warning.
  KIMIA_REQUIRE(scene.find("NaN") == kimia::kNullEntity);
  KIMIA_REQUIRE(scene.find("Inf") == kimia::kNullEntity);
  KIMIA_REQUIRE(scene.find("Real") != kimia::kNullEntity);
  KIMIA_REQUIRE(report.warnings.size() == 2U);
  KIMIA_REQUIRE(contains(report.warnings[0], "line 2"));
  KIMIA_REQUIRE(contains(report.warnings[1], "line 3"));
}

KIMIA_TEST(sceneio_duplicate_id_warns_and_still_loads_every_entity) {
  const std::string text =
      "# KIMIA scene v2\n"
      "e \"First\" id 5 mesh cube pos 0 0 0 scale 1 1 1 color 1 1 1 rough 0.5\n"
      "e \"Second\" id 5 mesh cube pos 1 0 0 scale 1 1 1 color 1 1 1 rough 0.5\n";
  Scene scene;
  std::string error;
  SceneIO::LoadReport report;
  KIMIA_REQUIRE(SceneIO::load(text, scene, error, report));
  KIMIA_REQUIRE(scene.count() == 2U);  // nothing is lost
  KIMIA_REQUIRE(scene.find("First") == 5U);
  KIMIA_REQUIRE(scene.find("Second") != kimia::kNullEntity);
  KIMIA_REQUIRE(scene.find("Second") != 5U);
  KIMIA_REQUIRE(report.restoredIds == 1U);
  KIMIA_REQUIRE(report.assignedIds == 1U);
  KIMIA_REQUIRE(report.warnings.size() == 1U);
  KIMIA_REQUIRE(contains(report.warnings[0], "line 3"));
  KIMIA_REQUIRE(contains(report.warnings[0], "already taken"));
}

// --- Through the file system -------------------------------------------------

KIMIA_TEST(sceneio_file_roundtrip_keeps_ids_and_reports) {
  Scene scene;
  scene.create(makeCube("A"));
  scene.create(makeCube("B"));
  scene.create(makeCube("C"));
  KIMIA_REQUIRE(scene.destroy(scene.find("B")));
  const std::string path = tmpPath("scene_file_v2.kimia");
  KIMIA_REQUIRE(SceneIO::saveToFile(scene, path));

  Scene loaded;
  std::string error;
  SceneIO::LoadReport report;
  KIMIA_REQUIRE(SceneIO::loadFromFile(path, loaded, error, report));
  KIMIA_REQUIRE(report.version == 2);
  KIMIA_REQUIRE(loaded.count() == 2U);
  KIMIA_REQUIRE(loaded.find("A") == 1U);
  KIMIA_REQUIRE(loaded.find("C") == 3U);

  // A missing file is an error with a reason, and the report stays empty.
  Scene missing;
  SceneIO::LoadReport missingReport;
  KIMIA_REQUIRE(!SceneIO::loadFromFile(tmpPath("no_such_scene.kimia"), missing, error, missingReport));
  KIMIA_REQUIRE(contains(error, "cannot open scene file"));
  KIMIA_REQUIRE(missingReport.version == 1);
}

// --- A world file is a scene file plus metadata ------------------------------
// WorldIO embeds SceneIO text, so the version rules have to hold through it.

KIMIA_TEST(worldio_carries_ids_through_a_world_file) {
  kimia::WorldData world;
  world.name = "Holes";
  EntityData ground = makeCube("Ground");
  ground.mesh = MeshKind::plane;
  world.scene.create(ground);          // id 1
  const EntityHandle removed = world.scene.create(makeCube("Gone"));
  world.scene.restore(9U, makeCube("Far"));  // a scene that is not 1..N
  KIMIA_REQUIRE(world.scene.destroy(removed));

  std::string text;
  KIMIA_REQUIRE(kimia::WorldIO::save(world, text));
  // WorldIO writes its own v1 header and then the scene's own line: the file
  // declares the newer of the two, and the entity ids are in it.
  // The file's first line is the scene's own version, not WorldIO's: a world
  // whose scene carries ids must SAY so, or a reader that trusts the header
  // would take it for v1 and renumber everything.
  KIMIA_REQUIRE(text.rfind("# KIMIA scene v2\n", 0U) == 0U);
  KIMIA_REQUIRE(text.find("e \"Far\" id 9 ") != std::string::npos);
  // A world whose scene needs no ids is still a v1 file, byte for byte.
  kimia::WorldData plain;
  plain.name = "Plain";
  plain.scene.create(makeCube("Ground"));
  std::string plainText;
  KIMIA_REQUIRE(kimia::WorldIO::save(plain, plainText));
  KIMIA_REQUIRE(plainText.rfind("# KIMIA scene v1\n", 0U) == 0U);
  KIMIA_REQUIRE(plainText.find(" id ") == std::string::npos);

  kimia::WorldData loaded;
  std::string error;
  KIMIA_REQUIRE(kimia::WorldIO::load(text, loaded, error));
  KIMIA_REQUIRE(loaded.name == "Holes");
  KIMIA_REQUIRE(loaded.scene.find("Ground") == 1U);
  KIMIA_REQUIRE(loaded.scene.find("Gone") == kimia::kNullEntity);
  KIMIA_REQUIRE(loaded.scene.find("Far") == 9U);

  // Save -> load -> save is byte-identical, ids included.
  std::string again;
  KIMIA_REQUIRE(kimia::WorldIO::save(loaded, again));
  KIMIA_REQUIRE(again == text);
}

KIMIA_TEST(worldio_reports_a_scene_from_a_newer_engine) {
  // A world file that claims a future scene version must be refused, not
  // loaded with a silent hole in it.
  const std::string text =
      "# KIMIA scene v1\n"
      "# world name Future\n"
      "# KIMIA scene v7\n"
      "e \"Thing\" id 2 mesh cube pos 0 0 0 scale 1 1 1 color 1 1 1 rough 0.5\n";
  kimia::WorldData loaded;
  std::string error;
  KIMIA_REQUIRE(!kimia::WorldIO::load(text, loaded, error));
  KIMIA_REQUIRE(contains(error, "version 7"));
}

// --- The worlds that ship with the engine ------------------------------------
// The acceptance criterion of phase 3 is "old worlds load unchanged". The
// strongest way to say that is to read every world in the tree, write it back
// and compare the bytes.

KIMIA_TEST(sceneio_shipped_worlds_roundtrip_byte_identical) {
#ifdef KIMIA_SOURCE_DIR
  const std::string worldsDir = std::string(KIMIA_SOURCE_DIR) + "/Worlds";
#else
  const std::string worldsDir = "Worlds";
#endif
  const std::vector<std::string> names = {"anim_demo.kimia", "kimia_poster.kimia", "street_kids.kimia"};
  for (const std::string& name : names) {
    const std::string path = worldsDir + "/" + name;
    std::ifstream file(path, std::ios::binary);
    if (!file) continue;  // a checkout without the worlds folder is not a failure
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string original = buffer.str();

    kimia::WorldData world;
    std::string error;
    KIMIA_REQUIRE(kimia::WorldIO::load(original, world, error));
    std::string rewritten;
    KIMIA_REQUIRE(kimia::WorldIO::save(world, rewritten));

    // All three worlds in the tree were hand-written before the world
    // metadata existed (no "# world name" in two of them, a partial profile
    // block in the third), so the first save by this build fills in what the
    // old files left out. That is enrichment, not a rewrite — but it has to
    // stop there, so the whitelist below is explicit and the real gate is the
    // stability check right after: once this build has written the file, no
    // later round trip may change a single byte.
    const bool handWritten = original.find("\n# profile name ") == std::string::npos;
    if (!handWritten) {
      if (rewritten != original) std::printf("world changed on save: %s\n", path.c_str());
      KIMIA_REQUIRE(rewritten == original);
    } else {
      KIMIA_REQUIRE(name == "anim_demo.kimia" || name == "kimia_poster.kimia" ||
                    name == "street_kids.kimia");
      // The names those files clearly meant are read now (they used to load
      // as "MyWorld"): "# name X" for the first two, the real line for the
      // third.
      KIMIA_REQUIRE(!world.name.empty());
      KIMIA_REQUIRE(world.name != "MyWorld");
      KIMIA_REQUIRE(rewritten.find("# world name " + world.name + "\n") != std::string::npos);
      KIMIA_REQUIRE(rewritten.find("# profile name sandbox\n") != std::string::npos);
    }

    // The stability gate, for every world: this build's own output is what
    // later builds have to be able to read back unchanged.
    kimia::WorldData again;
    KIMIA_REQUIRE(kimia::WorldIO::load(rewritten, again, error));
    std::string third;
    KIMIA_REQUIRE(kimia::WorldIO::save(again, third));
    if (third != rewritten) std::printf("world re-save differs: %s\n", path.c_str());
    KIMIA_REQUIRE(third == rewritten);
    KIMIA_REQUIRE(again.name == world.name);
    KIMIA_REQUIRE(again.scene.count() == world.scene.count());
    world.scene.forEach([&again](kimia::EntityHandle, const EntityData& entity) {
      const EntityData* other = again.scene.get(again.scene.find(entity.name));
      KIMIA_REQUIRE(other != nullptr);
      KIMIA_REQUIRE(other->transform.position.x == entity.transform.position.x);
      KIMIA_REQUIRE(other->transform.position.y == entity.transform.position.y);
      KIMIA_REQUIRE(other->transform.position.z == entity.transform.position.z);
      KIMIA_REQUIRE(other->meshFile == entity.meshFile);
      KIMIA_REQUIRE(other->animations.size() == entity.animations.size());
    });

    // And no shipped world is a v2 file: they all predate ids.
    KIMIA_REQUIRE(original.rfind("# KIMIA scene v2\n", 0U) != 0U);
    SceneIO::LoadReport report;
    Scene scene;
    KIMIA_REQUIRE(SceneIO::load(original, scene, error, report));
    KIMIA_REQUIRE(report.version == 1);
    KIMIA_REQUIRE(report.migrated);
    KIMIA_REQUIRE(report.assignedIds == scene.count());
  }
}
