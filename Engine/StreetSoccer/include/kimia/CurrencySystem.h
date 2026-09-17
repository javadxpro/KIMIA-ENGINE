#pragma once
// =============================================================================
//  CurrencySystem — 4-tier in-game currency: Coin, Token, Gem, Star.
//
//  Required by phase 1: "سیستم ارز ۴ لایه".
//
//  Reward tiers (engagement -> rarity):
//    Coin  - drops every match; trivial.   10..200 per game.
//    Token - daily / season reward.        1..5 per day.
//    Gem   - high-skill demonstration.    0..2 per match (skill-gated).
//    Star  - top-tier only.               0..1 per 10 matches.
//
//  Anti-DDA principle preserved: these reward player SKILL, not progression
//  time. A poor player still gets coins; they just don't earn gems/stars
//  until their effective skill passes the gating threshold. This is the
//  opposite of an XP-grind: coins are participation, stars are mastery.
// =============================================================================

#include <kimia/Types.h>
#include <cstdint>

namespace kimia::street {

enum class CurrencyKind : u8 {
  Coin  = 0,
  Token = 1,
  Gem   = 2,
  Star  = 3,
};

struct Wallet {
  u32 coin  = 0;
  u32 token = 0;
  u32 gem   = 0;
  u32 star  = 0;

  u64 get(CurrencyKind k) const {
    switch (k) {
      case CurrencyKind::Coin:  return coin;
      case CurrencyKind::Token: return token;
      case CurrencyKind::Gem:   return gem;
      case CurrencyKind::Star:  return star;
    }
    return 0;
  }

  void add(CurrencyKind k, u32 amount) {
    switch (k) {
      case CurrencyKind::Coin:  coin  += amount; break;
      case CurrencyKind::Token: token += amount; break;
      case CurrencyKind::Gem:   gem   += amount; break;
      case CurrencyKind::Star:  star  += amount; break;
    }
  }
};

// Reward policy. Pure data; tested in CurrencySystemTests.
struct CurrencyAward {
  u32 coins        = 0;
  u32 tokens       = 0;
  u32 gems         = 0;
  u32 stars        = 0;
};

// Tunables. Skill gate > 0.50 means above-50%-EMA passes the gate.
struct CurrencyTuning {
  f32 gemSkillGate  = 0.50f;
  f32 starSkillGate = 0.75f;
  u32 coinPerGoal   = 30;
  u32 coinPerMatch  = 50;        // participation reward
  u32 tokenPerGoal  = 1;
};

// Compute the award a player gets at end of match from skill EMA,
// goals scored, and whether they won/drew.
CurrencyAward computeAward(f32 effectiveSkillEma,
                           u32 goalsScored,
                           bool won,
                           bool drew,
                           const CurrencyTuning& tune = CurrencyTuning{});

// Apply the award to a wallet in-place.
void applyAward(Wallet& w, const CurrencyAward& a);

}  // namespace kimia::street
