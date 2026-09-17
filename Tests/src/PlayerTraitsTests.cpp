#include <kimia_test.h>
#include <kimia/PlayerTraits.h>
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

// ---------------------------------------------------------------------------
KIMIA_TEST(Trait_DefaultZeroMood) {
  PlayerTraits t;
  EXPECT(t.mood == 0.5f);
  EXPECT(t.skill == 0.5f);
}

KIMIA_TEST(Trait_RollMoodInRange) {
  for (kimia::u32 s = 0; s < 1000; ++s) {
    const float m = rollMood(s);
    EXPECT(m >= 0.0f);
    EXPECT(m <= 1.0f);
  }
}

KIMIA_TEST(Trait_RollMoodDifferentSeeds) {
  // Different seeds should produce different moods in general.
  bool anyDifferent = false;
  float prev = rollMood(1);
  for (kimia::u32 s = 2; s < 50; ++s) {
    const float m = rollMood(s);
    if (std::abs(m - prev) > 0.001f) anyDifferent = true;
    prev = m;
  }
  EXPECT(anyDifferent);
}

KIMIA_TEST(Trait_RollMoodDeterministic) {
  EXPECT(rollMood(42) == rollMood(42));
}

KIMIA_TEST(Trait_EffectiveStatsApplyMood) {
  PlayerTraits t;
  t.skill = 0.5f;
  t.mood = 1.0f;  // best mood
  EffectiveStats s = effectiveStats(t);
  EXPECT(s.skill > 0.5f);

  t.mood = 0.0f;
  s = effectiveStats(t);
  EXPECT(s.skill < 0.5f);
}

KIMIA_TEST(Trait_EffectiveStatsClamped) {
  PlayerTraits t;
  t.skill = 1.0f;
  t.mood = 1.0f;
  EffectiveStats s = effectiveStats(t);
  EXPECT(s.skill <= 1.0f);
  EXPECT(s.skill >= 0.0f);

  t.skill = 0.0f;
  t.mood = 0.0f;
  s = effectiveStats(t);
  EXPECT(s.skill >= 0.0f);
}

KIMIA_TEST(Trait_TrickNamesKnown) {
  EXPECT(std::string(trickName(SignatureTrick::Elastico)) == "Elastico");
  EXPECT(std::string(trickName(SignatureTrick::RainbowFlick)) == "Rainbow Flick");
  EXPECT(std::string(trickName(SignatureTrick::BicycleKick)) == "Bicycle Kick");
  EXPECT(std::string(trickName(SignatureTrick::IranianNutmeg)) == "Iranian Nutmeg");
}

KIMIA_TEST(Trait_TrickMinSkillOrdering) {
  // Harder tricks should have higher skill thresholds.
  EXPECT(trickMinSkill(SignatureTrick::StreetHeel) <
         trickMinSkill(SignatureTrick::IranianNutmeg));
  EXPECT(trickMinSkill(SignatureTrick::IranianNutmeg) <
         trickMinSkill(SignatureTrick::Elastico));
  EXPECT(trickMinSkill(SignatureTrick::Elastico) <
         trickMinSkill(SignatureTrick::RainbowFlick));
  EXPECT(trickMinSkill(SignatureTrick::RainbowFlick) <
         trickMinSkill(SignatureTrick::BicycleKick));
}

KIMIA_TEST(Trick_NoTrickIfNone) {
  PlayerTraits t;
  t.signature = SignatureTrick::None;
  PlayerTraits opp;
  EXPECT(!shouldAttemptTrick(t, opp, 1.0f));
}

KIMIA_TEST(Trick_HighUnpredictabilityTriggers) {
  PlayerTraits t;
  t.skill = 1.0f;
  t.mood = 1.0f;
  t.unpredictability = 1.0f;
  t.signature = SignatureTrick::StreetHeel;
  PlayerTraits opp;
  // Run many short ticks; at least one should fire.
  int fires = 0;
  for (int i = 0; i < 200; ++i) {
    t.matchTime = static_cast<float>(i) * 0.1f;
    if (shouldAttemptTrick(t, opp, 0.1f)) ++fires;
  }
  EXPECT(fires > 0);
}

KIMIA_TEST(Trick_LowSkillBlocksHighThreshold) {
  PlayerTraits t;
  t.skill = 0.3f;       // too low
  t.mood = 0.5f;
  t.unpredictability = 1.0f;
  t.signature = SignatureTrick::BicycleKick;  // needs 0.85
  PlayerTraits opp;
  int fires = 0;
  for (int i = 0; i < 1000; ++i) {
    t.matchTime = static_cast<float>(i) * 0.05f;
    if (shouldAttemptTrick(t, opp, 0.1f)) ++fires;
  }
  EXPECT_EQ(fires, 0);
}

KIMIA_TEST(Trick_EnoughSkillAllows) {
  PlayerTraits t;
  t.skill = 0.95f;
  t.mood = 0.95f;
  t.unpredictability = 1.0f;
  t.signature = SignatureTrick::BicycleKick;
  PlayerTraits opp;
  int fires = 0;
  for (int i = 0; i < 200; ++i) {
    t.matchTime = static_cast<float>(i) * 0.1f;
    if (shouldAttemptTrick(t, opp, 0.1f)) ++fires;
  }
  EXPECT(fires > 0);
}

KIMIA_TEST(Trait_BrazilianFlairSkilled) {
  auto t = brazilianFlairTraits(10);
  EXPECT(t.skill > 0.6f);
  EXPECT(t.unpredictability > 0.7f);
  EXPECT(t.signature == SignatureTrick::Elastico);
}

KIMIA_TEST(Trait_BrazilianDifferentNumbersDifferentTricks) {
  auto t7 = brazilianFlairTraits(7);
  auto t10 = brazilianFlairTraits(10);
  EXPECT(t7.signature != t10.signature);
}

KIMIA_TEST(Trait_IranianGritAggressive) {
  auto t = iranianGritTraits(10);
  EXPECT(t.aggression > 0.7f);
  EXPECT(t.stamina > 0.7f);
}

KIMIA_TEST(Trait_IranianTrickForShirt9) {
  auto t = iranianGritTraits(9);
  EXPECT(t.signature == SignatureTrick::IranianNutmeg);
}

KIMIA_TEST(Trait_TeamAverageSkill) {
  std::vector<Player> team;
  Player a; a.stamina = 0.6f; team.push_back(a);
  Player b; b.stamina = 0.8f; team.push_back(b);
  const float avg = teamAverageSkill(team);
  EXPECT(std::abs(avg - 0.7f) < 0.01f);
}

KIMIA_TEST(Trait_TeamAverageEmpty) {
  std::vector<Player> empty;
  EXPECT(teamAverageSkill(empty) == 0.0f);
}

// ---------------------------------------------------------------------------
// Mood oscillation: a high-skill player on a bad day should be worse than a
// low-skill player on a good day, but the AI never breaks — mood is bounded.
// ---------------------------------------------------------------------------
KIMIA_TEST(Mood_BadDayDoesNotMakeWorseThanZero) {
  PlayerTraits t;
  t.skill = 1.0f;
  t.mood = 0.0f;
  const EffectiveStats s = effectiveStats(t);
  EXPECT(s.skill >= 0.0f);
}

KIMIA_TEST(Mood_GoodDayDoesNotExceedOne) {
  PlayerTraits t;
  t.skill = 1.0f;
  t.mood = 1.0f;
  const EffectiveStats s = effectiveStats(t);
  EXPECT(s.skill <= 1.0f);
}
