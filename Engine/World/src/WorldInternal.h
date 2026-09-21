#pragma once

// The World module's own toolbox: the small helpers and tuning constants its
// translation units share. PRIVATE to Engine/World/src — it is not installed,
// not included outside this folder, and nothing in it is API.
//
// Why this file exists: these used to live in an anonymous namespace at the
// top of World.cpp. That worked while World.cpp was the only file, and stopped
// working the moment the editor was split up (Documentation/Architecture.md):
// a helper in an anonymous namespace belongs to ONE translation unit, so the
// first file that needed `kMoveEpsilon` or `turnToward` had to either copy it
// (two definitions of one piece of knowledge, which is how things drift) or
// re-declare it. One header, one definition, included by every World .cpp.
//
// Every name here is an implementation detail, which is why they live in
// `worldinternal` and not in `kimia`: the public surface stays World.h.

#include <kimia/Scene.h>
#include <kimia/Types.h>
#include <kimia/Vec.h>

#include <cmath>
#include <cstring>
#include <map>
#include <string>

namespace kimia {
namespace worldinternal {

inline constexpr f64 kGoalCelebration = 2.0;  // seconds after a goal (or a holed ball)

// Which side owns a goal, and therefore which side is punished by it. The
// goal on the far half (-Z) is team 2's net, so scoring there is team 1's
// goal; the one on the player's half (+Z) is team 1's net. A goal exactly on
// the halfway line (z == 0) counts for team 1: the classic single-goal
// kickabout every world built so far shoots toward -Z.
inline u32 scoringTeamForGoalZ(f64 goalZ) { return goalZ <= 0.0 ? 1U : 2U; }

inline constexpr f64 kKickMaxSpeed = 8.0;  // a ball rolling faster is not re-kicked
inline constexpr f64 kMoveEpsilon = 1e-6;
inline constexpr f64 kPlayerMargin = 0.6;  // keep the player inside the floor
inline constexpr f64 kEditMargin = 0.5;    // ghost / moved objects stay this far from the edge

// Turn `from` toward `to` by at most `maxStep` radians, the short way round.
// Used by the character motor: a body that turned the long way (or spun past
// its target on a fast frame) would look like a bug, and angles are modular,
// so "which way is shorter" is a real question.
inline f64 turnToward(f64 from, f64 to, f64 maxStep) {
  constexpr f64 kTurn = 6.28318530717958647692;  // 2 pi
  f64 delta = std::fmod(to - from, kTurn);
  if (delta > kTurn * 0.5) delta -= kTurn;
  if (delta < -kTurn * 0.5) delta += kTurn;
  if (std::abs(delta) <= maxStep) return to;
  return from + (delta > 0.0 ? maxStep : -maxStep);
}

inline const char* screenName(int screen) {
  switch (screen) {
    case 0: return "MAIN";
    case 1: return "BUILDER";
    case 2: return "CATALOG";
    case 3: return "PLAYER";
    case 4: return "BALL";
    case 5: return "BLOCK";
    case 6: return "WALLLEN";
    case 7: return "WALLAXIS";
    case 8: return "GOALQ";
    case 9: return "PLACE";
    case 10: return "MANAGE";
    case 11: return "MOVE";
    case 12: return "DELETE";
    case 13: return "COLOR";
    case 14: return "ENV";
    case 15: return "PLAY";
    case 16: return "GOAL";
    case 17: return "MODELFILE";
    case 18: return "MODELSIZE";
    case 19: return "INSPECTOR";
    case 20: return "PROFILE";
    default: return "ROUNDEND";
  }
}

inline const char* playerSpeedName(f64 speed) {
  if (speed >= kWorldPlayerFast - 0.5) return "fast";
  if (speed <= kWorldPlayerSlow + 0.5) return "slow";
  return "normal";
}

inline bool endsWith(const std::string& text, const char* suffix) {
  const usize length = std::strlen(suffix);
  return text.size() >= length && text.compare(text.size() - length, length, suffix) == 0;
}

inline std::string baseName(const std::string& path) {
  const usize slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1U);
}

// One goal = a "Goal*" entity (single cube: scale.x = width, scale.y = 2) or
// a legacy trio GoalPostLeft/GoalPostRight/GoalBar. Legacy parts share the
// same base name, so they collapse into one group.
inline std::string goalBase(const std::string& name) {
  static const char* kSuffixes[] = {"PostLeft", "PostRight", "Bar"};
  for (const char* suffix : kSuffixes) {
    const usize length = std::strlen(suffix);
    if (name.size() > length && name.compare(name.size() - length, length, suffix) == 0) {
      return name.substr(0, name.size() - length);
    }
  }
  return name;
}

struct GoalGroup {
  bool hasSingle = false;
  Vec3 singlePos{0.0, 0.0, 0.0};
  Vec3 singleScale{1.0, 1.0, 1.0};
  bool hasPosts = false;
  Vec3 leftPos{0.0, 0.0, 0.0};
  Vec3 rightPos{0.0, 0.0, 0.0};
  Vec3 barPos{0.0, 0.0, 0.0};
  bool valid() const { return hasSingle || hasPosts; }
  f64 z() const { return hasSingle ? singlePos.z : barPos.z; }
  f64 x() const { return hasSingle ? singlePos.x : (leftPos.x + rightPos.x) * 0.5; }
  f64 width() const { return hasSingle ? singleScale.x : std::abs(rightPos.x - leftPos.x); }
  f64 height() const { return hasSingle ? singlePos.y + singleScale.y * 0.5 : barPos.y; }
};

inline void scanGoals(const Scene& scene, std::map<std::string, GoalGroup>& groups) {
  groups.clear();
  scene.forEach([&groups](EntityHandle, const EntityData& entity) {
    if (entity.name.rfind("Goal", 0) != 0) return;
    GoalGroup& group = groups[goalBase(entity.name)];
    if (endsWith(entity.name, "PostLeft")) {
      group.hasPosts = true;
      group.leftPos = entity.transform.position;
    } else if (endsWith(entity.name, "PostRight")) {
      group.hasPosts = true;
      group.rightPos = entity.transform.position;
    } else if (endsWith(entity.name, "Bar")) {
      group.hasPosts = true;
      group.barPos = entity.transform.position;
    } else {
      group.hasSingle = true;
      group.singlePos = entity.transform.position;
      group.singleScale = entity.transform.scale;
    }
  });
}

}  // namespace worldinternal
}  // namespace kimia
