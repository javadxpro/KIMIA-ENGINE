#pragma once
// =============================================================================
//  Player Traits (Brazil Football Street / فوتبال خیابونی ایران)
//
//  Each player has a stable trait profile (set at creation) plus a daily
//  mood that fluctuates. The combination of stable trait + transient mood
//  makes the AI feel alive without scripting randomness.
//
//  Traits:
//    aggression     — how often the player attempts tackles/sprints
//    skill          — baseline technical quality (pass, dribble, shoot)
//    stamina        — base pool; drains faster when mood is low
//    unpredictability — how often the player attempts signature tricks
//
//  Mood fluctuates around 0..1 per match:
//    0.0 — off day; everything is a bit harder
//    0.5 — neutral
//    1.0 — on fire; sharpens every stat by ~20%
//
//  Signature trick is per-player; each has a single signature move that
//  triggers when:
//    - player has the ball
//    - unpredictability check passes
//    - skill level + mood is above the trick's threshold
// =============================================================================

#include <kimia/StreetSoccer.h>
#include <kimia/Types.h>
#include <string>
#include <vector>

namespace kimia::street {

enum class SignatureTrick {
  None,
  Elastico,        // Ronaldinho-style flick
  RainbowFlick,    // Neymar-style
  BicycleKick,     // rare; high skill required
  CruyffTurn,      // Johan Cruyff legacy
  HocusPocus,      // stepover series
  PuskasTrick,     // Puskas scissors
  StreetHeel,      // street variant, low-skill
  IranianNutmeg,   // ایرانی خاص، low-skill trick
};

const char* trickName(SignatureTrick t);
f32 trickMinSkill(SignatureTrick t);

// -----------------------------------------------------------------------------
struct PlayerTraits {
  f32 aggression       = 0.5f;
  f32 skill            = 0.5f;
  f32 stamina          = 0.7f;
  f32 unpredictability = 0.3f;
  SignatureTrick signature = SignatureTrick::None;
  f32 mood = 0.5f;
  f32 matchTime = 0.0f;
};

// -----------------------------------------------------------------------------
struct EffectiveStats {
  f32 aggression       = 0.5f;
  f32 skill            = 0.5f;
  f32 stamina          = 0.7f;
  f32 unpredictability = 0.3f;
  f32 shotAccuracyBonus = 0.0f;
  f32 speedBonus        = 0.0f;
};

// -----------------------------------------------------------------------------
f32 rollMood(u32 seed);
EffectiveStats effectiveStats(const PlayerTraits& t);
bool shouldAttemptTrick(const PlayerTraits& t,
                        const PlayerTraits& opponentTrait,
                        f32 dt);
PlayerTraits brazilianFlairTraits(u8 shirtNumber);
PlayerTraits iranianGritTraits(u8 shirtNumber);
f32 teamAverageSkill(const std::vector<Player>& team);

}  // namespace kimia::street
