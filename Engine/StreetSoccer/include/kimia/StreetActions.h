#pragma once
// =============================================================================
//  StreetActions — repertoire of move-the-ball actions for Iran Street Soccer.
//
//  Required by phase-2 trailer scope:
//    - "پشت پا تک ضرب"
//    - "پاس هوایی"
//    - "دریبل خاص گل"
//    - "دروازه‌بان شوت به دیوار و گل"
//    - "سر هوایی"
//
//  Each action is a sequence of micro-impulses that the ball or the players
//  follow. The SequenceAction is data, not code, so the AI controller, the
//  player-input driver, and the cinematic replay script all reuse the same
//  spec.
//
//  Using a small DSL (sequence of (which input, delay, magnitude)) keeps
//  everything deterministic and observable.
// =============================================================================

#include <kimia/Types.h>
#include <string>
#include <vector>

namespace kimia::street {

enum class StreetActionKind : u8 {
  BackheelTap       = 0,  // پشت پا تک ضرب
  FirstTimeVolley   = 1,  // تک ضرب هوایی
  BicycleHeader     = 2,  // دوچرخه هوایی با سر
  GroundCross       = 3,  // سانر زمینی
  ChipPass          = 4,  // پاس چیپ هوایی
  KeeperWallGoal    = 5,  // شوت دروازه‌بان به دیوار، گل
  WheelBreakDribble = 6,  // دریبل با چرخش
  SombreroFlick     = 7,  // سومبررو فیک
  TallManNutmeg     = 8,  // ناتمگ قدبلند
  FreeKickTopCorner = 9,  // پنالتی بالای دیوار
};

struct ActionStep {
  f32   at           = 0.0f;   // seconds from action start
  f32   impulseX     = 0.0f;   // m/s along pitch X (← − / → +)
  f32   impulseY     = 0.0f;   // m/s vertical (Y+)
  f32   impulseZ     = 0.0f;   // m/s along pitch width
  bool  toBall       = true;   // apply to ball (else to a player index)
  u8    playerIndex  = 0;      // for player-targeted impulses
  f32   sprintMul    = 1.0f;   // sprint multiplier during this impulse
  bool  triggersGoal = false;  // end-of-action if true
};

struct StreetAction {
  StreetActionKind kind;
  std::string      labelFa;
  std::string      labelEn;
  f32              durationSeconds = 0.0f;
  f32              successThreshold = 0.0f;  // skill EMA required
  std::vector<ActionStep> steps;
};

// Library of all known actions.
const std::vector<StreetAction>& streetActionLibrary();
const StreetAction* findAction(StreetActionKind k);
const char* actionLabelFa(StreetActionKind k);
const char* actionLabelEn(StreetActionKind k);

// Roll whether an action succeeds given skill EMA, then return its steps.
// On failure, return an empty vector (no action).
std::vector<ActionStep> rollStreetAction(StreetActionKind k,
                                         f32 skillEma,
                                         u32* rngState);

}  // namespace kimia::street
