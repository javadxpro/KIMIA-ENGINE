#include <kimia_test.h>
#include <kimia/StreetSoccer.h>
#include <cmath>
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

KIMIA_TEST(StreetSoccer_BuildDefaultTeams) {
  MatchState m;
  buildDefaultTeams(m);
  EXPECT_EQ(m.homeTeam.size(), 4u);
  EXPECT_EQ(m.awayTeam.size(), 4u);
  EXPECT_EQ(m.homeScore, 0);
  EXPECT_EQ(m.awayScore, 0);
}

KIMIA_TEST(StreetSoccer_GoalkeeperAssigned) {
  MatchState m;
  buildDefaultTeams(m);
  bool homeGK = false, awayGK = false;
  for (auto& p : m.homeTeam) if (p.isGoalkeeper) homeGK = true;
  for (auto& p : m.awayTeam) if (p.isGoalkeeper) awayGK = true;
  EXPECT(homeGK);
  EXPECT(awayGK);
}

KIMIA_TEST(StreetSoccer_BallStopsByFriction) {
  Ball b;
  b.vx = 10.0f;
  b.vy = 5.0f;
  for (int i = 0; i < 100; ++i) {
    tickBall(b, 1.0f / 60.0f);
  }
  EXPECT(std::abs(b.vx) < 1.0f);
  EXPECT(std::abs(b.vy) < 1.0f);
}

KIMIA_TEST(StreetSoccer_BallMovesFromVelocity) {
  Ball b;
  b.x = 0; b.y = 0;
  b.vx = 6.0f; b.vy = 0;
  tickBall(b, 1.0f);
  EXPECT(b.x > 5.0f);
}

KIMIA_TEST(StreetSoccer_HomeGoalDetected) {
  MatchState m;
  buildDefaultTeams(m);
  m.ball.x = m.pitch.length * 0.5f + 0.1f;
  m.ball.y = 0.0f;
  std::vector<GoalEvent> newGoals;
  detectGoals(m, newGoals);
  EXPECT_EQ(newGoals.size(), 1u);
  EXPECT(newGoals[0].scoredBy == TeamSide::Home);
}

KIMIA_TEST(StreetSoccer_AwayGoalDetected) {
  MatchState m;
  buildDefaultTeams(m);
  m.ball.x = -m.pitch.length * 0.5f - 0.1f;
  m.ball.y = 0.0f;
  std::vector<GoalEvent> newGoals;
  detectGoals(m, newGoals);
  EXPECT_EQ(newGoals.size(), 1u);
  EXPECT(newGoals[0].scoredBy == TeamSide::Away);
}

KIMIA_TEST(StreetSoccer_GoalMissedIfTooWide) {
  MatchState m;
  buildDefaultTeams(m);
  m.ball.x = m.pitch.length * 0.5f + 0.1f;
  m.ball.y = m.pitch.goalWidth;
  std::vector<GoalEvent> newGoals;
  detectGoals(m, newGoals);
  EXPECT_EQ(newGoals.size(), 0u);
}

KIMIA_TEST(StreetSoccer_ResetMatchClearsGoals) {
  MatchState m;
  buildDefaultTeams(m);
  m.homeScore = 3;
  m.awayScore = 1;
  resetMatch(m);
  EXPECT_EQ(m.homeScore, 0);
  EXPECT_EQ(m.awayScore, 0);
  EXPECT(m.ball.x == 0.0f);
  EXPECT(m.ball.y == 0.0f);
  EXPECT(!m.playing);
}

KIMIA_TEST(StreetSoccer_ResetRestoresStamina) {
  MatchState m;
  buildDefaultTeams(m);
  for (auto& p : m.homeTeam) p.stamina = 0.0f;
  resetMatch(m);
  for (auto& p : m.homeTeam) EXPECT(p.stamina > 0.0f);
}

KIMIA_TEST(StreetSoccer_PlayersOnTheirHalf) {
  MatchState m;
  buildDefaultTeams(m);
  for (auto& p : m.homeTeam) EXPECT(p.positionX <= 0.0f);
  for (auto& p : m.awayTeam) EXPECT(p.positionX >= 0.0f);
}

KIMIA_TEST(StreetSoccer_PitchDimensions) {
  MatchState m;
  buildDefaultTeams(m);
  EXPECT(m.pitch.length > 0.0f);
  EXPECT(m.pitch.width  > 0.0f);
  EXPECT(m.pitch.goalWidth > 0.0f);
  EXPECT(m.pitch.goalWidth < m.pitch.width);
}

KIMIA_TEST(StreetSoccer_BallDefaultMass) {
  MatchState m;
  buildDefaultTeams(m);
  EXPECT(m.ball.mass > 0.3f);
  EXPECT(m.ball.mass < 0.6f);
  EXPECT(m.ball.radius > 0.05f);
  EXPECT(m.ball.radius < 0.20f);
}

KIMIA_TEST(StreetSoccer_DefaultTopSpeed) {
  MatchState m;
  buildDefaultTeams(m);
  for (auto& p : m.homeTeam) {
    if (!p.isGoalkeeper) EXPECT(p.speed >= 5.0f);
  }
}
