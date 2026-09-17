#include <kimia/PlayerTraits.h>
#include <cmath>

namespace kimia::street {

const char* trickName(SignatureTrick t) {
  switch (t) {
    case SignatureTrick::None:           return "None";
    case SignatureTrick::Elastico:       return "Elastico";
    case SignatureTrick::RainbowFlick:   return "Rainbow Flick";
    case SignatureTrick::BicycleKick:    return "Bicycle Kick";
    case SignatureTrick::CruyffTurn:     return "Cruyff Turn";
    case SignatureTrick::HocusPocus:     return "Hocus Pocus";
    case SignatureTrick::PuskasTrick:    return "Puskas Scissors";
    case SignatureTrick::StreetHeel:     return "Street Heel";
    case SignatureTrick::IranianNutmeg:  return "Iranian Nutmeg";
  }
  return "?";
}

f32 trickMinSkill(SignatureTrick t) {
  // 0..1 threshold combining skill + mood.
  switch (t) {
    case SignatureTrick::None:           return 1.1f; // never
    case SignatureTrick::StreetHeel:     return 0.40f;
    case SignatureTrick::IranianNutmeg:   return 0.45f;
    case SignatureTrick::HocusPocus:     return 0.55f;
    case SignatureTrick::PuskasTrick:    return 0.60f;
    case SignatureTrick::Elastico:       return 0.65f;
    case SignatureTrick::CruyffTurn:     return 0.70f;
    case SignatureTrick::RainbowFlick:   return 0.75f;
    case SignatureTrick::BicycleKick:    return 0.85f;
  }
  return 1.0f;
}

namespace {
// Tiny xorshift for mood rolls.
u32 xorshift(u32 x) {
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}
}

f32 rollMood(u32 seed) {
  // Combine seed with clock-derived noise so each match gives a different mood.
  const u32 r = xorshift(seed ? seed : 0x9E3779B9u);
  return (r & 0xFFFF) / 65535.0f;
}

EffectiveStats effectiveStats(const PlayerTraits& t) {
  EffectiveStats s;
  // Mood in [-1, +1] scaled to ±20%.
  const f32 moodBoost = (t.mood - 0.5f) * 0.4f;
  s.aggression       = std::max(0.0f, std::min(1.0f, t.aggression + moodBoost));
  s.skill            = std::max(0.0f, std::min(1.0f, t.skill + moodBoost));
  s.stamina          = std::max(0.0f, std::min(1.0f, t.stamina + moodBoost * 0.5f));
  s.unpredictability = std::max(0.0f, std::min(1.0f,
                                  t.unpredictability + moodBoost * 0.5f));
  s.shotAccuracyBonus = moodBoost * 0.10f;
  s.speedBonus        = moodBoost * 0.20f;
  return s;
}

bool shouldAttemptTrick(const PlayerTraits& t,
                        const PlayerTraits& opponentTrait,
                        f32 dt) {
  if (t.signature == SignatureTrick::None) return false;
  // Probability per second.
  const f32 perSec = t.unpredictability * 0.20f;
  const f32 chance = perSec * dt;
  // Roll deterministic for replay: combine traits + time buckets.
  const u32 seed = static_cast<u32>(t.skill * 1000.0f) ^ static_cast<u32>(t.mood * 7919.0f)
                 ^ static_cast<u32>(opponentTrait.skill * 421.0f);
  const f32 mood = rollMood(seed + static_cast<u32>(t.matchTime * 4.0f));
  if (mood > chance) return false;
  // Skill + mood gate.
  const f32 skillGate = trickMinSkill(t.signature);
  return (t.skill + t.mood * 0.3f) >= skillGate;
}

PlayerTraits brazilianFlairTraits(u8 n) {
  PlayerTraits t;
  t.aggression       = 0.45f;
  t.skill            = 0.75f;
  t.stamina          = 0.65f;
  t.unpredictability = 0.85f;
  switch (n) {
    case 1:  t.signature = SignatureTrick::None; break;
    case 7:  t.signature = SignatureTrick::HocusPocus; break;
    case 10: t.signature = SignatureTrick::Elastico; break;
    case 11: t.signature = SignatureTrick::RainbowFlick; break;
    default: t.signature = SignatureTrick::StreetHeel;
  }
  return t;
}

PlayerTraits iranianGritTraits(u8 n) {
  PlayerTraits t;
  t.aggression       = 0.80f;
  t.skill            = 0.65f;
  t.stamina          = 0.85f;
  t.unpredictability = 0.45f;
  switch (n) {
    case 1:  t.signature = SignatureTrick::None; break;
    case 9:  t.signature = SignatureTrick::IranianNutmeg; break;
    case 10: t.signature = SignatureTrick::CruyffTurn; break;
    case 23: t.signature = SignatureTrick::PuskasTrick; break;
    default: t.signature = SignatureTrick::StreetHeel;
  }
  return t;
}

f32 teamAverageSkill(const std::vector<Player>& team) {
  if (team.empty()) return 0.0f;
  f32 sum = 0.0f;
  for (const auto& p : team) sum += p.stamina;  // proxy
  return sum / static_cast<f32>(team.size());
}

}  // namespace kimia::street
