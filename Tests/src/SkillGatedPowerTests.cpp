#include <kimia_test.h>
#include <kimia/SkillGatedPower.h>
#include <cstdio>

using namespace kimia::street;

static int g_pass = 0;
static int g_fail = 0;
#define EXPECT(cond) do { \
  if (cond) { ++g_pass; } \
  else      { ++g_fail; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } \
} while (0)
#define EXPECT_EQ(a, b) do { \
  auto va = (a); auto vb = (b); \
  if (va == vb) { ++g_pass; } \
  else { ++g_fail; std::printf("FAIL %s:%d %s == %s\n", __FILE__, __LINE__, #a, #b); } \
} while (0)

// Use plain double for tests, cast where f32 is required.
static inline bool activeP(const PowerState& s, StreetPower p) {
  return powerTimeRemaining(s, p) > 0.0f;
}
static inline bool cdP(const PowerState& s, StreetPower p) {
  return powerCooldownRemaining(s, p) > 0.0f;
}

// ---------------------------------------------------------------------------
KIMIA_TEST(ComboBurst_RequiresLosingByTwoGoals) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.scoreDiff = -1;
  EXPECT(!tryActivatePower(s, StreetPower::ComebackBurst, ctx));
}

KIMIA_TEST(ComboBurst_ActivatesWhenLosingByTwoOrMore) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.scoreDiff = -2;
  EXPECT(tryActivatePower(s, StreetPower::ComebackBurst, ctx));
  EXPECT(activeP(s, StreetPower::ComebackBurst));
}

KIMIA_TEST(ComboBurst_DoesNotActivateWhenWinning) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.scoreDiff = 2;
  EXPECT(!tryActivatePower(s, StreetPower::ComebackBurst, ctx));
}

KIMIA_TEST(ComboBurst_DurationIsTwoSeconds) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx; ctx.scoreDiff = -2;
  tryActivatePower(s, StreetPower::ComebackBurst, ctx);
  EXPECT(powerTimeRemaining(s, StreetPower::ComebackBurst) > 1.99f);
  EXPECT(powerTimeRemaining(s, StreetPower::ComebackBurst) <= 2.0f);
}

KIMIA_TEST(ComboBurst_ExpiresAfterDuration) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx; ctx.scoreDiff = -2;
  tryActivatePower(s, StreetPower::ComebackBurst, ctx);
  for (int i = 0; i < 180; ++i) tickPower(s, 1.0f / 60.0f);
  EXPECT(!activeP(s, StreetPower::ComebackBurst));
}

KIMIA_TEST(ComboBurst_GoesOnCooldown) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx; ctx.scoreDiff = -2;
  tryActivatePower(s, StreetPower::ComebackBurst, ctx);
  EXPECT(cdP(s, StreetPower::ComebackBurst));
  EXPECT(!tryActivatePower(s, StreetPower::ComebackBurst, ctx));
}

KIMIA_TEST(ComboBurst_CooldownClearsAfter60Seconds) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx; ctx.scoreDiff = -2;
  tryActivatePower(s, StreetPower::ComebackBurst, ctx);
  for (int i = 0; i < 3700; ++i) tickPower(s, 1.0f / 60.0f);
  EXPECT(!cdP(s, StreetPower::ComebackBurst));
}

KIMIA_TEST(ComboBurst_SpeedMultiplier) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx; ctx.scoreDiff = -2;
  tryActivatePower(s, StreetPower::ComebackBurst, ctx);
  const double baseSpeed = 6.0;
  const double boost = effectiveSpeedMultiplier(s, static_cast<float>(baseSpeed));
  EXPECT(boost > baseSpeed * 1.3);
  EXPECT(boost <= baseSpeed * 1.5);
}

KIMIA_TEST(ComboBurst_NoBoostWhenNotActive) {
  PowerState s = makeDefaultPowerState();
  const float speed = effectiveSpeedMultiplier(s, 6.0f);
  EXPECT(speed == 6.0f);
}

// ---------------------------------------------------------------------------
KIMIA_TEST(LastStand_RequiresLast30PctAndLosing) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.scoreDiff = -1;
  ctx.timeRemaining = 30.0f;
  ctx.totalTime = 180.0f;
  EXPECT(tryActivatePower(s, StreetPower::LastStand, ctx));
}

KIMIA_TEST(LastStand_DoesNotActivateEarlyInMatch) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.scoreDiff = -2;
  ctx.timeRemaining = 90.0f;
  ctx.totalTime = 180.0f;
  EXPECT(!tryActivatePower(s, StreetPower::LastStand, ctx));
}

KIMIA_TEST(LastStand_DoesNotActivateWhenTied) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.scoreDiff = 0;
  ctx.timeRemaining = 30.0f;
  ctx.totalTime = 180.0f;
  EXPECT(!tryActivatePower(s, StreetPower::LastStand, ctx));
}

KIMIA_TEST(LastStand_ShotPowerBoost) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.scoreDiff = -1;
  ctx.timeRemaining = 30.0f;
  ctx.totalTime = 180.0f;
  tryActivatePower(s, StreetPower::LastStand, ctx);
  EXPECT(effectiveShotMultiplier(s, 1.0f) > 1.4f);
}

// ---------------------------------------------------------------------------
KIMIA_TEST(CrowdBoost_ActivatesOnTie) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.tiedScore = true;
  EXPECT(tryActivatePower(s, StreetPower::CrowdBoost, ctx));
}

KIMIA_TEST(CrowdBoost_StaminaHalved) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.tiedScore = true;
  tryActivatePower(s, StreetPower::CrowdBoost, ctx);
  EXPECT(effectiveStaminaMultiplier(s, 1.0f) < 0.6f);
}

KIMIA_TEST(CrowdBoost_5SecondsDuration) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.tiedScore = true;
  tryActivatePower(s, StreetPower::CrowdBoost, ctx);
  EXPECT(powerTimeRemaining(s, StreetPower::CrowdBoost) > 4.99f);
  EXPECT(powerTimeRemaining(s, StreetPower::CrowdBoost) <= 5.0f);
}

// ---------------------------------------------------------------------------
KIMIA_TEST(StreetSense_ActivatesWhenOppNearOwnGoal) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.opponentHasBallNearOwnGoal = true;
  EXPECT(tryActivatePower(s, StreetPower::StreetSense, ctx));
}

KIMIA_TEST(StreetSense_DoublesVision) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.opponentHasBallNearOwnGoal = true;
  tryActivatePower(s, StreetPower::StreetSense, ctx);
  EXPECT(effectiveVisionMultiplier(s, 10.0f) > 19.0f);
}

// ---------------------------------------------------------------------------
KIMIA_TEST(DoubleOrNothing_RequiresThreePasses) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.consecutivePasses = 2;
  EXPECT(!tryActivatePower(s, StreetPower::DoubleOrNothing, ctx));
  ctx.consecutivePasses = 3;
  EXPECT(tryActivatePower(s, StreetPower::DoubleOrNothing, ctx));
}

KIMIA_TEST(DoubleOrNothing_ShotBoost) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.consecutivePasses = 3;
  tryActivatePower(s, StreetPower::DoubleOrNothing, ctx);
  EXPECT(effectiveShotMultiplier(s, 1.0f) > 1.2f);
}

// ---------------------------------------------------------------------------
KIMIA_TEST(Mastery_StartsAtZero) {
  PowerState s = makeDefaultPowerState();
  EXPECT(s.metrics.masteryComeback == 0.0f);
}

KIMIA_TEST(Mastery_GrowsOnSuccessfulUse) {
  PowerState s = makeDefaultPowerState();
  for (int i = 0; i < 10; ++i) {
    recordPowerOutcome(s, StreetPower::ComebackBurst, true);
  }
  EXPECT(s.metrics.masteryComeback > 0.3f);
}

KIMIA_TEST(Mastery_CappedAtOne) {
  PowerState s = makeDefaultPowerState();
  for (int i = 0; i < 100; ++i) {
    recordPowerOutcome(s, StreetPower::ComebackBurst, true);
  }
  EXPECT(s.metrics.masteryComeback <= 1.0f);
}

KIMIA_TEST(Mastery_WastedUseCapsLower) {
  PowerState s = makeDefaultPowerState();
  for (int i = 0; i < 10; ++i) {
    recordPowerOutcome(s, StreetPower::ComebackBurst, true);
  }
  const double before = s.metrics.masteryComeback;
  for (int i = 0; i < 10; ++i) {
    recordPowerOutcome(s, StreetPower::ComebackBurst, false);
  }
  EXPECT(s.metrics.masteryComeback < before);
  EXPECT(s.metrics.masteryComeback <= 0.5);
}

// ---------------------------------------------------------------------------
KIMIA_TEST(Metrics_ShotAccuracyEma) {
  SkillMetrics m;
  for (int i = 0; i < 100; ++i) recordShot(m, true);
  EXPECT(m.shotAccuracy > 0.95f);
}

KIMIA_TEST(Metrics_PassSuccessEma) {
  SkillMetrics m;
  for (int i = 0; i < 100; ++i) recordPass(m, false);
  EXPECT(m.passSuccess < 0.05f);
}

KIMIA_TEST(Metrics_MovementEfficiencyClamped) {
  SkillMetrics m;
  recordMovement(m, 10.0f, 5.0f);
  EXPECT(m.movementEfficiency > 0.5f);
}

KIMIA_TEST(Metrics_MovementEfficiencyZero) {
  SkillMetrics m;
  recordMovement(m, 0.0f, 10.0f);
  EXPECT(m.movementEfficiency < 0.5f);
}

KIMIA_TEST(Metrics_ZeroTotalIsSafe) {
  SkillMetrics m;
  recordMovement(m, 0.0f, 0.0f);
  EXPECT(m.movementEfficiency == 0.5f);
}

// ---------------------------------------------------------------------------
KIMIA_TEST(Hud_EmptyWhenNoActivePowers) {
  PowerState s = makeDefaultPowerState();
  EXPECT(powerHudString(s).empty());
}

KIMIA_TEST(Hud_ShowsActivePower) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx; ctx.scoreDiff = -2;
  tryActivatePower(s, StreetPower::ComebackBurst, ctx);
  const std::string hud = powerHudString(s);
  EXPECT(!hud.empty());
  EXPECT(hud.find("Comeback Burst") != std::string::npos);
}

KIMIA_TEST(Hud_DecreasesWithTime) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx; ctx.scoreDiff = -2;
  tryActivatePower(s, StreetPower::ComebackBurst, ctx);
  const std::string before = powerHudString(s);
  tickPower(s, 0.5f);
  const std::string after = powerHudString(s);
  EXPECT(before != after);
}

// ---------------------------------------------------------------------------
KIMIA_TEST(Edge_EmptyContextNoPowerActivates) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  EXPECT(!tryActivatePower(s, StreetPower::ComebackBurst, ctx));
  EXPECT(!tryActivatePower(s, StreetPower::LastStand, ctx));
  EXPECT(!tryActivatePower(s, StreetPower::CrowdBoost, ctx));
  EXPECT(!tryActivatePower(s, StreetPower::StreetSense, ctx));
  EXPECT(!tryActivatePower(s, StreetPower::DoubleOrNothing, ctx));
}

KIMIA_TEST(Edge_LastStandWithZeroTotalTime) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.scoreDiff = -1;
  ctx.timeRemaining = 30.0f;
  ctx.totalTime = 0.0f;
  EXPECT(!tryActivatePower(s, StreetPower::LastStand, ctx));
}

KIMIA_TEST(Edge_MultiplePowersActiveSimultaneously) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx;
  ctx.scoreDiff = -2;
  ctx.consecutivePasses = 3;
  tryActivatePower(s, StreetPower::ComebackBurst, ctx);
  tryActivatePower(s, StreetPower::DoubleOrNothing, ctx);
  EXPECT(activeP(s, StreetPower::ComebackBurst));
  EXPECT(activeP(s, StreetPower::DoubleOrNothing));
  EXPECT(effectiveSpeedMultiplier(s, 6.0f) > 8.0f);
  EXPECT(effectiveShotMultiplier(s, 1.0f) > 1.2f);
}

KIMIA_TEST(Edge_TickWithZeroDtSafe) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx; ctx.scoreDiff = -2;
  tryActivatePower(s, StreetPower::ComebackBurst, ctx);
  tickPower(s, 0.0f);
  EXPECT(activeP(s, StreetPower::ComebackBurst));
}

KIMIA_TEST(Edge_TickNegativeDtSafe) {
  PowerState s = makeDefaultPowerState();
  TriggerContext ctx; ctx.scoreDiff = -2;
  tryActivatePower(s, StreetPower::ComebackBurst, ctx);
  tickPower(s, -1.0f);
  EXPECT(powerTimeRemaining(s, StreetPower::ComebackBurst) >= 0.0f);
}

KIMIA_TEST(Edge_PowerNameKnown) {
  EXPECT(std::string(powerName(StreetPower::ComebackBurst)) == "Comeback Burst");
}

KIMIA_TEST(Edge_PowerNameForAll) {
  EXPECT(std::string(powerName(StreetPower::ComebackBurst)).length() > 0);
  EXPECT(std::string(powerName(StreetPower::LastStand)).length() > 0);
  EXPECT(std::string(powerName(StreetPower::CrowdBoost)).length() > 0);
  EXPECT(std::string(powerName(StreetPower::StreetSense)).length() > 0);
  EXPECT(std::string(powerName(StreetPower::DoubleOrNothing)).length() > 0);
}
