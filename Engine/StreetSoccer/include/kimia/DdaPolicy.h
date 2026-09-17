#pragma once
// =============================================================================
//  DdaPolicy — explicit policy statements: what the engine WILL and WILL NOT do.
//
//  Required by phase 1: "سیستم DDA متفاوت".
//
//  Unlike classic dynamic-difficulty-adjustment, our policy refuses to
//  silently tweak damage / accuracy / speed. We will NOT reduce enemy
//  damage when the player is losing; that violates the project's
//  anti-DDA contract. Comeback Burst is a separate player-chosen reward
//  with a fixed trigger condition and a mastery check. Other adjustments
//  that DO happen (and are explicitly listed):
//
//    * RandomEvents — environmental chaos that affects both teams equally.
//    * Mode tuning — players opt into Comedy / Grass / Speed.
//
//  This file makes those policies testable.
// =============================================================================

#include <kimia/Types.h>

namespace kimia::street {

struct DdaPolicy {
  bool silentAdjustmentsAllowed() const { return false; }
  bool comebackBurstRequiresMastery() const { return true; }
  i32 comebackBurstDeficitThreshold() const { return 2; }
  f32 comebackBurstMinMastery() const { return 0.5f; }
  bool randomEventsSymmetric() const { return true; }
  const char* description() const {
    return "No silent DDA. Comeback Burst is reward, not adjustment. "
           "Random events are equal for both teams.";
  }
};

}  // namespace kimia::street
