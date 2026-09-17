#include <kimia_test.h>
#include <kimia/HighlightCapture.h>
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
#define ASSERT(cond) EXPECT(cond)
#define ASSERT_EQ(a, b) EXPECT_EQ(a, b)

KIMIA_TEST(Highlight_DefaultWindow) {
  HighlightCapture c;
  EXPECT(c.highlights().empty());
}

KIMIA_TEST(Highlight_SetWindow) {
  HighlightCapture c;
  c.setWindow(3.0f, 7.0f);
}

KIMIA_TEST(Highlight_TickWithoutGoalDoesNothing) {
  MatchState m;
  buildDefaultTeams(m);
  HighlightCapture c;
  c.tick(m, 0.0f);
  c.tick(m, 1.0f);
  EXPECT(c.highlights().empty());
}

KIMIA_TEST(Highlight_DetectsHomeGoal) {
  MatchState before;
  buildDefaultTeams(before);
  MatchState after = before;
  after.homeScore = 1;
  TeamSide side;
  EXPECT(HighlightCapture::detectGoal(before, after, side));
  EXPECT(side == TeamSide::Home);
}

KIMIA_TEST(Highlight_DetectsAwayGoal) {
  MatchState before;
  buildDefaultTeams(before);
  MatchState after = before;
  after.awayScore = 1;
  TeamSide side;
  EXPECT(HighlightCapture::detectGoal(before, after, side));
  EXPECT(side == TeamSide::Away);
}

KIMIA_TEST(Highlight_NoGoalDetected) {
  MatchState before;
  buildDefaultTeams(before);
  MatchState after = before;
  TeamSide side;
  EXPECT(!HighlightCapture::detectGoal(before, after, side));
}

KIMIA_TEST(Highlight_TickDetectsGoalAndStores) {
  MatchState m;
  buildDefaultTeams(m);
  HighlightCapture c;
  c.tick(m, 0.0f);  // initial state
  m.homeScore = 1;
  c.tick(m, 5.0f);
  EXPECT_EQ(c.highlights().size(), 1u);
}

KIMIA_TEST(Highlight_MultipleGoalsDetected) {
  MatchState m;
  buildDefaultTeams(m);
  HighlightCapture c;
  c.tick(m, 0.0f);
  m.homeScore = 1;
  c.tick(m, 1.0f);
  m.awayScore = 1;
  c.tick(m, 2.0f);
  m.homeScore = 2;
  c.tick(m, 3.0f);
  EXPECT_EQ(c.highlights().size(), 3u);
}

KIMIA_TEST(Highlight_GoalHasTenSecondWindow) {
  MatchState m;
  buildDefaultTeams(m);
  HighlightCapture c;
  c.setWindow(5.0f, 5.0f);
  c.tick(m, 0.0f);
  m.homeScore = 1;
  c.tick(m, 30.0f);
  ASSERT(!c.highlights().empty());
  EXPECT(c.highlights().front().startTime == 25.0f);
  EXPECT(c.highlights().front().endTime   == 35.0f);
}

KIMIA_TEST(Highlight_GoalAttribution) {
  MatchState m;
  buildDefaultTeams(m);
  HighlightCapture c;
  c.tick(m, 0.0f);
  // Simulate scorer is Garrincha (shirt 7).
  m.homeTeam[1].hasBall = true;
  m.homeScore = 1;
  m.goals.push_back({TeamSide::Home, 0.0f});
  c.tick(m, 5.0f);
  ASSERT_EQ(c.highlights().size(), 1u);
  // Name should come from homeTeam where hasBall == true.
  EXPECT(c.highlights().front().scorer == "Garrincha");
}

KIMIA_TEST(Highlight_ClearEmpties) {
  MatchState m;
  buildDefaultTeams(m);
  HighlightCapture c;
  c.tick(m, 0.0f);
  m.homeScore = 1;
  c.tick(m, 1.0f);
  EXPECT(!c.highlights().empty());
  c.clear();
  EXPECT(c.highlights().empty());
}

KIMIA_TEST(Highlight_MakeGoalHighlight) {
  MatchState m;
  buildDefaultTeams(m);
  Highlight h = HighlightCapture::makeGoalHighlight(TeamSide::Home, m, "Pelezinho", 100.0f);
  EXPECT(h.kind == HighlightKind::Goal);
  EXPECT(h.homeScore == 0);
  EXPECT(h.scorer == "Pelezinho");
  EXPECT(h.startTime == 95.0f);
  EXPECT(h.endTime   == 105.0f);
}

KIMIA_TEST(Highlight_MakeGoalHighlightAway) {
  MatchState m;
  buildDefaultTeams(m);
  Highlight h = HighlightCapture::makeGoalHighlight(TeamSide::Away, m, "Azmoun", 200.0f);
  EXPECT(h.scorer == "Azmoun");
}

KIMIA_TEST(Highlight_TickWithoutFirstDoesNothing) {
  // First call is internal bookkeeping; a single call should not produce.
  MatchState m;
  buildDefaultTeams(m);
  m.homeScore = 5;  // pre-set
  HighlightCapture c;
  c.tick(m, 10.0f);
  EXPECT(c.highlights().empty());
}

KIMIA_TEST(Highlight_LongMatchManyGoals) {
  MatchState m;
  buildDefaultTeams(m);
  HighlightCapture c;
  c.tick(m, 0.0f);
  for (int i = 0; i < 10; ++i) {
    if (i % 2 == 0) m.homeScore++;
    else            m.awayScore++;
    c.tick(m, static_cast<float>(i + 1) * 30.0f);
  }
  EXPECT_EQ(c.highlights().size(), 10u);
}
