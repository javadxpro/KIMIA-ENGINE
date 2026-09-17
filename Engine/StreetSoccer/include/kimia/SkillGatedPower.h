#pragma once
// =============================================================================
//  Skill-Gated Power (Brazil Football Street / فوتبال خیابونی ایران)
//
//  Comeback Burst is the headline power: a 2-second speed boost (+40%) granted
//  to a player who is currently LOSING — but only if they can demonstrate the
//  skill to use it. Mastery is tracked per-power: triggering the power without
//  knowing what to do with it (wrong direction, wrong moment) wastes it.
//
//  This is NOT classic DDA:
//    - Skill-Gated Power never makes the game easier or harder artificially.
//    - It only rewards demonstrated skill with a temporary advantage.
//    - A bad player gets NO boost; an expert player gets the boost they earned.
//
//  Design contract:
//    1. The system NEVER resolves a goal or outcome automatically.
//    2. Activation is purely informational (HUD + sound).
//    3. The boost is multiplicative on the player's speed; if the player
//       doesn't use it effectively, the boost expires wasted.
// =============================================================================

#include <kimia/StreetSoccer.h>
#include <kimia/Types.h>
#include <string>
#include <vector>

namespace kimia::street {

enum class StreetPower {
  ComebackBurst,    // غیرتی مود: 2s ×1.4 speed when losing by ≥2
  LastStand,        // in last 30s and losing: shot power ×1.5
  CrowdBoost,       // tied score: stamina drain halved for 5s
  StreetSense,      // opponent has ball near own goal: vision range ×2
  DoubleOrNothing,  // after 3 successful passes: next shot ×1.3
};

const char* powerName(StreetPower p);

// -----------------------------------------------------------------------------
// Per-power tuning
// -----------------------------------------------------------------------------
struct PowerTuning {
  f32  durationSec      = 2.0f;
  f32  cooldownSec      = 60.0f;
  f32  speedMultiplier  = 1.4f;
  f32  shotMultiplier   = 1.0f;
  f32  staminaMultiplier = 1.0f;
  f32  visionMultiplier = 1.0f;
};

// -----------------------------------------------------------------------------
// Match metrics — tracked over a rolling window of recent matches.
// -----------------------------------------------------------------------------
struct SkillMetrics {
  f32 shotAccuracy    = 0.5f;   // 0..1
  f32 passSuccess     = 0.5f;
  f32 possessionRetention = 0.5f;
  f32 tackleSuccess   = 0.5f;
  f32 movementEfficiency = 0.5f;
  // Mastery level per power (0..1). Increases with successful activations.
  f32 masteryComeback    = 0.0f;
  f32 masteryLastStand   = 0.0f;
  f32 masteryCrowd       = 0.0f;
  f32 masteryStreetSense = 0.0f;
  f32 masteryDouble      = 0.0f;
};

// -----------------------------------------------------------------------------
// Activation record — what fired, when, and how well it was used.
// -----------------------------------------------------------------------------
struct PowerActivation {
  StreetPower power = StreetPower::ComebackBurst;
  f32 remainingSec  = 0.0f;
  f32 cooldownSec   = 0.0f;
  bool active       = false;
};

// -----------------------------------------------------------------------------
// System state — owned by the AI module per-player.
// -----------------------------------------------------------------------------
struct PowerState {
  SkillMetrics metrics;
  std::vector<PowerActivation> active;
  std::vector<PowerActivation> cooldown;

  // HUD info (read-only).
  StreetPower lastTrigger = StreetPower::ComebackBurst;
  f32  lastTriggerMastery = 0.0f;
  bool lastTriggerUsable  = false;
};

// -----------------------------------------------------------------------------
// Trigger conditions — pass into tickPower() each frame.
// -----------------------------------------------------------------------------
struct TriggerContext {
  i32 scoreDiff = 0;             // positive = team ahead
  f32 timeRemaining = 0.0f;      // seconds
  f32 totalTime = 0.0f;          // for "last 30%" check
  bool opponentHasBallNearOwnGoal = false;
  i32 consecutivePasses = 0;     // for DoubleOrNothing
  bool tiedScore = false;
};

// -----------------------------------------------------------------------------
// API
// -----------------------------------------------------------------------------

// Build a default PowerState (one activation slot per power + cooldowns).
PowerState makeDefaultPowerState();

// Tick power activations: decrement active timers, decrement cooldowns.
void tickPower(PowerState& s, f32 dt);

// Try to fire a specific power given current match context.
// Returns true if the power was activated.
bool tryActivatePower(PowerState& s,
                      StreetPower p,
                      const TriggerContext& ctx,
                      const PowerTuning& tuning = PowerTuning{});

// Mastery update — call when the player used (or wasted) a power.
void recordPowerOutcome(PowerState& s,
                        StreetPower p,
                        bool usedEffectively);

// Update skill metrics — call from gameplay events.
void recordShot       (SkillMetrics& m, bool onTarget);
void recordPass       (SkillMetrics& m, bool succeeded);
void recordPossession (SkillMetrics& m, bool retained);
void recordTackle     (SkillMetrics& m, bool succeeded);
void recordMovement   (SkillMetrics& m, f32 useful, f32 total);

// Effective speed multiplier — includes active powers.
f32 effectiveSpeedMultiplier(const PowerState& s, f32 baseSpeed);

// Effective shot power multiplier — for LastStand / DoubleOrNothing.
f32 effectiveShotMultiplier(const PowerState& s, f32 baseShot);

// Effective stamina drain multiplier — for CrowdBoost.
f32 effectiveStaminaMultiplier(const PowerState& s, f32 baseDrain);

// Effective vision multiplier — for StreetSense.
f32 effectiveVisionMultiplier(const PowerState& s, f32 baseVision);

// HUD helper — returns a short status string for the active power.
std::string powerHudString(const PowerState& s);

// Inspection helpers (for tests and HUD).
bool isPowerActive(const PowerState& s, StreetPower p);
bool isPowerOnCooldown(const PowerState& s, StreetPower p);
f32  powerTimeRemaining(const PowerState& s, StreetPower p);
f32  powerCooldownRemaining(const PowerState& s, StreetPower p);

}  // namespace kimia::street
