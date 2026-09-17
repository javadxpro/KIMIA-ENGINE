// Real impl of CurrencySystem.
#include <kimia/CurrencySystem.h>

namespace kimia::street {

CurrencyAward computeAward(f32 effectiveSkillEma,
                          u32 goalsScored, bool won, bool drew,
                          const CurrencyTuning& tune) {
  CurrencyAward a;
  a.coins = tune.coinPerMatch + tune.coinPerGoal * goalsScored;
  if (won)  a.coins += 25;
  if (drew) a.coins += 10;
  a.tokens = tune.tokenPerGoal * goalsScored;
  if (won)  a.tokens += 2;
  if (drew) a.tokens += 1;
  if (a.tokens > 6) a.tokens = 6;
  if (effectiveSkillEma >= tune.gemSkillGate) {
    a.gems = 1;
    if (effectiveSkillEma >= 0.7f) a.gems = 2;
  }
  if (won && effectiveSkillEma >= tune.starSkillGate) {
    a.stars = 1;
  }
  return a;
}

void applyAward(Wallet& w, const CurrencyAward& a) {
  w.add(CurrencyKind::Coin,  a.coins);
  w.add(CurrencyKind::Token, a.tokens);
  w.add(CurrencyKind::Gem,   a.gems);
  w.add(CurrencyKind::Star,  a.stars);
}

}  // namespace kimia::street
