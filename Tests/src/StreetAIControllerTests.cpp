#include <kimia_test.h>
#include <kimia/StreetAIController.h>
#include <kimia/StreetSoccerPhysics.h>
#include <kimia/Physics.h>
#include <cstdio>

using namespace kimia::street;
using namespace kimia;

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

// ---------------------------------------------------------------------------
KIMIA_TEST(AI_DefaultTraitsHome) {
  auto t = defaultTraitsForSide(TeamSide::Home, 10);
  EXPECT(t.unpredictability > 0.7f);
  EXPECT(t.signature == SignatureTrick::Elastico);
}

KIMIA_TEST(AI_DefaultTraitsAway) {
  auto t = defaultTraitsForSide(TeamSide::Away, 9);
  EXPECT(t.aggression > 0.7f);
  EXPECT(t.signature == SignatureTrick::IranianNutmeg);
}

KIMIA_TEST(AI_TickDoesNotCrash) {
  MatchState m;
  buildDefaultTeams(m);
  Player p = m.homeTeam[0];
  PlayerTraits t = defaultTraitsForSide(TeamSide::Home, p.shirtNumber);
  AIDecision d = tickAI(m, p, t, 0, TeamSide::Home, 1.0f / 60.0f);
  (void)d;
}

KIMIA_TEST(AI_HasBallDecidesToScore) {
  MatchState m;
  buildDefaultTeams(m);
  m.homeTeam[1].positionX = m.pitch.length * 0.4f;
  m.homeTeam[1].hasBall   = true;
  PlayerTraits t = defaultTraitsForSide(TeamSide::Home, 7);
  AIDecision d = tickAI(m, m.homeTeam[1], t, 1, TeamSide::Home, 1.0f / 60.0f);
  // In opponent third, aggression skill decides; with default traits we
  // expect at least some velocity toward +X (away goal).
  EXPECT(d.desiredVx > 0.0f);
}

KIMIA_TEST(AI_GoalkeeperStaysClose) {
  MatchState m;
  buildDefaultTeams(m);
  // Move keeper far away.
  m.homeTeam[0].positionX = -20.0f;
  m.homeTeam[0].positionY = 5.0f;
  PlayerTraits t = defaultTraitsForSide(TeamSide::Home, 1);
  AIDecision d = tickAI(m, m.homeTeam[0], t, 0, TeamSide::Home, 1.0f / 60.0f);
  // Should head back toward home.
  EXPECT(d.desiredVx < 0.0f);
}

KIMIA_TEST(AI_AggressivePlayerChasesBall) {
  MatchState m;
  buildDefaultTeams(m);
  // Aggressive Iranian player.
  Player p = m.awayTeam[1];
  p.positionX = 0;
  p.positionY = 0;
  m.ball.x = 5.0f; m.ball.y = 0.0f;
  PlayerTraits t = iranianGritTraits(p.shirtNumber);
  AIDecision d = tickAI(m, p, t, 1, TeamSide::Away, 1.0f / 60.0f);
  // Should move toward ball.
  EXPECT(d.desiredVx > 0.0f);
}

KIMIA_TEST(AI_KickDecisionProducesImpulse) {
  MatchState m;
  buildDefaultTeams(m);
  Player p = m.homeTeam[1];
  PlayerTraits t = defaultTraitsForSide(TeamSide::Home, p.shirtNumber);
  AIDecision d = kickDecision(p, t, m, 1.0f / 60.0f);
  EXPECT(d.wantsKick);
  EXPECT(d.kickPower > 0.0f);
}

KIMIA_TEST(AI_KickDirectionHomeIsPositiveX) {
  MatchState m;
  buildDefaultTeams(m);
  Player p = m.homeTeam[1];
  PlayerTraits t = defaultTraitsForSide(TeamSide::Home, p.shirtNumber);
  AIDecision d = kickDecision(p, t, m, 1.0f / 60.0f);
  EXPECT(d.desiredVx > 0.0f);
}

KIMIA_TEST(AI_KickDirectionAwayIsNegativeX) {
  MatchState m;
  buildDefaultTeams(m);
  Player p = m.awayTeam[1];
  PlayerTraits t = defaultTraitsForSide(TeamSide::Away, p.shirtNumber);
  AIDecision d = kickDecision(p, t, m, 1.0f / 60.0f);
  EXPECT(d.desiredVx < 0.0f);
}

// ---------------------------------------------------------------------------
KIMIA_TEST(Match_TickAdvancesTime) {
  MatchState m;
  buildDefaultTeams(m);
  PhysicsWorld world;
  StreetPhysicsBridge bridge;
  setupStreetPhysics(bridge, world, m.pitch);
  spawnBallBody(bridge, world, m.ball);
  for (auto& pl : m.homeTeam) spawnPlayerBody(bridge, world, pl);
  for (auto& pl : m.awayTeam) spawnPlayerBody(bridge, world, pl);
  const float before = m.matchTime;
  tickMatch(m, bridge, 1.0f);
  EXPECT(m.matchTime > before);
}

KIMIA_TEST(Match_TickAdvancesPhysics) {
  MatchState m;
  buildDefaultTeams(m);
  PhysicsWorld world;
  StreetPhysicsBridge bridge;
  setupStreetPhysics(bridge, world, m.pitch);
  spawnBallBody(bridge, world, m.ball);
  for (auto& pl : m.homeTeam) spawnPlayerBody(bridge, world, pl);
  for (auto& pl : m.awayTeam) spawnPlayerBody(bridge, world, pl);
  // Apply a kick then tick.
  kickBall(bridge, 30.0f, 0.0f);
  const float xBefore = m.ball.x;
  tickMatch(m, bridge, 1.0f);
  EXPECT(m.ball.x != xBefore || m.ball.vx != 0.0f);
}

KIMIA_TEST(Match_GoalIncrementsScore) {
  MatchState m;
  buildDefaultTeams(m);
  PhysicsWorld world;
  StreetPhysicsBridge bridge;
  setupStreetPhysics(bridge, world, m.pitch);
  spawnBallBody(bridge, world, m.ball);
  for (auto& pl : m.homeTeam) spawnPlayerBody(bridge, world, pl);
  for (auto& pl : m.awayTeam) spawnPlayerBody(bridge, world, pl);
  // Teleport ball just past the goal line.
  teleportBall(bridge, m.pitch.length * 0.5f + 0.2f, 0.0f);
  tickMatch(m, bridge, 0.5f);
  EXPECT(m.homeScore >= 1);
}

KIMIA_TEST(Match_ScoreDiffOutMatches) {
  MatchState m;
  buildDefaultTeams(m);
  m.homeScore = 3; m.awayScore = 1;
  PhysicsWorld world;
  StreetPhysicsBridge bridge;
  setupStreetPhysics(bridge, world, m.pitch);
  spawnBallBody(bridge, world, m.ball);
  for (auto& pl : m.homeTeam) spawnPlayerBody(bridge, world, pl);
  for (auto& pl : m.awayTeam) spawnPlayerBody(bridge, world, pl);
  i32 diff = 0;
  tickMatch(m, bridge, 0.5f, &diff);
  EXPECT(diff == 2);
}

KIMIA_TEST(Match_ResetClearsGoalsAndTime) {
  MatchState m;
  buildDefaultTeams(m);
  m.homeScore = 2;
  m.matchTime = 99.0f;
  resetMatch(m);
  EXPECT(m.homeScore == 0);
  EXPECT(m.matchTime == 0.0f);
}

KIMIA_TEST(Match_FullSimulationTwoSeconds) {
  MatchState m;
  buildDefaultTeams(m);
  PhysicsWorld world;
  StreetPhysicsBridge bridge;
  setupStreetPhysics(bridge, world, m.pitch);
  spawnBallBody(bridge, world, m.ball);
  for (auto& pl : m.homeTeam) spawnPlayerBody(bridge, world, pl);
  for (auto& pl : m.awayTeam) spawnPlayerBody(bridge, world, pl);
  // Drive for 2 simulated seconds at 60Hz.
  for (int i = 0; i < 120; ++i) {
    tickMatch(m, bridge, 1.0f / 60.0f);
  }
  EXPECT(m.matchTime > 1.5f);
  // Ball may or may not be in motion but should be inside pitch.
  EXPECT(std::abs(m.ball.x) <= m.pitch.length * 0.6f);
  EXPECT(std::abs(m.ball.y) <= m.pitch.width  * 0.6f);
}

KIMIA_TEST(Match_GoalLogsEvent) {
  MatchState m;
  buildDefaultTeams(m);
  PhysicsWorld world;
  StreetPhysicsBridge bridge;
  setupStreetPhysics(bridge, world, m.pitch);
  spawnBallBody(bridge, world, m.ball);
  for (auto& pl : m.homeTeam) spawnPlayerBody(bridge, world, pl);
  for (auto& pl : m.awayTeam) spawnPlayerBody(bridge, world, pl);
  EXPECT(m.goals.empty());
  teleportBall(bridge, m.pitch.length * 0.5f + 0.2f, 0.0f);
  tickMatch(m, bridge, 0.5f);
  EXPECT(!m.goals.empty());
  EXPECT(m.goals.back().scoredBy == TeamSide::Home);
}
