#pragma once

#include <kimia/Scene.h>

#include <string>
#include <vector>

namespace kimia {

// KIMIA scene text format.
//
//   # KIMIA scene v1
//   e "Green" mesh plane pos 0 0 0 scale 1 1 1 color 0.22 0.45 0.24 rough 0.95
//   # KIMIA scene v2
//   e "Wall" id 7 mesh cube pos 2.4 0.5 0 scale 0.5 1 4.4 color 0.7 0.68 0.62
//   # KIMIA scene v3
//   e "Player" mesh cube motor 4 24 0.25 4.9 12 pos 0 0.5 4 color 0.2 0.5 0.9 rough 0.5
//   # demo 0.000000 0.610000
//
// Rules:
// - `#` lines are comments, EXCEPT `# KIMIA scene vN` (the file version) and
//   `# demo <aim> <power>` (the player-authored demo shot).
// - mesh is one of: cube, plane, sphere.
// - entity names are double-quoted (`\"` and `\\` are the escapes); a bare
//   token is also accepted on load.
// - Unknown keywords/tokens are skipped and partial entity lines are ignored
//   (tolerant load) — but they are REPORTED (see LoadReport), because a
//   silently smaller scene is how "my object disappeared" bugs live.
//
// Versioning (Documentation/Scene.md has the reasoning):
// - v1 is the format every existing world is written in: entity lines, no
//   ids. Loading such a file assigns handles in file order, which is exactly
//   the numbering v1 always produced.
// - v2 adds `id <n>` per entity, so a scene whose handles are not 1..N (an
//   object was deleted, or a stage kept its ids) survives save -> load
//   unchanged.
// - v3 adds the character motor (`motor <speed> <accel> <air> <jump> <turn>`).
//   It is a version of its own because an engine that predates it would load
//   the file, drop the motor and then walk the character at the world's speed
//   without saying so — the exact silent loss the version rule exists to stop.
// - The writer emits the OLDEST version that can express the scene, so a
//   scene that does not need ids still saves byte-identically to v1 and an
//   older engine can still read it.
// - The reader accepts every version up to kVersion and REFUSES a file from a
//   newer one, instead of quietly dropping what it does not understand.
class SceneIO {
public:
  // The version this build writes, and the newest it can read.
  static constexpr int kVersion = 3;
  // The version that introduced each feature, so a writer can declare the
  // oldest one that can express the scene in front of it.
  static constexpr int kIdVersion = 2;     // `id <n>` per entity
  static constexpr int kMotorVersion = 3;  // `motor <speed> <accel> <air> <jump> <turn>`

  // What a load saw. Warnings carry line numbers, so a broken file can be
  // shown to the person who made it (phase 10 validation leans on this).
  struct LoadReport {
    int version = 1;      // the version the file declares (1 when it declares none)
    // The load had to invent identity for something: a file with no ids (every
    // v1 file) was numbered here in line order, or a duplicate id was repaired.
    // A v2 file read by a v3 build is NOT a migration — everything in it is
    // understood as written.
    bool migrated = false;
    usize restoredIds = 0U;  // entities that kept the id stored in the file
    usize assignedIds = 0U;  // entities that got the next free id instead
    std::vector<std::string> ignoredKeywords;  // sorted, unique
    std::vector<std::string> warnings;
  };

  static bool save(const Scene& scene, std::string& out);
  static bool saveToFile(const Scene& scene, const std::string& path);

  // Always succeeds for well-formed text of a known version; tolerates
  // anything it does not understand. `error` is only set for IO failures and
  // for a file from a newer version than this build understands.
  static bool load(const std::string& text, Scene& out, std::string& error);
  static bool load(const std::string& text, Scene& out, std::string& error, LoadReport& report);
  static bool loadFromFile(const std::string& path, Scene& out, std::string& error);
  static bool loadFromFile(const std::string& path, Scene& out, std::string& error, LoadReport& report);
};

}  // namespace kimia
