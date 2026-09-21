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
// - The writer emits the OLDEST version that can express the scene, so a
//   scene that does not need ids still saves byte-identically to v1 and an
//   older engine can still read it.
// - The reader accepts v1 and v2 and REFUSES a file from a newer version,
//   instead of quietly dropping what it does not understand.
class SceneIO {
public:
  // The version this build writes, and the newest it can read.
  static constexpr int kVersion = 2;

  // What a load saw. Warnings carry line numbers, so a broken file can be
  // shown to the person who made it (phase 10 validation leans on this).
  struct LoadReport {
    int version = 1;      // the version the file declares (1 when it declares none)
    bool migrated = false;  // the file was older than kVersion
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
