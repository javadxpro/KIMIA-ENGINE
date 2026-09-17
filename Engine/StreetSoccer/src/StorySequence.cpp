// StorySequence.cpp — narrative event generator for street-soccer matches.

#include <kimia/StorySequence.h>

#include <cstdio>

namespace kimia::street {

// ------------------------------------------------------------------ helpers

static const char* fa_intro =
    "ظهر توي کوچه‌ي ابوذر، صدای توپ پلاستيکي مياد";
static const char* fa_outro =
    "سوت پايان، سايه‌ها بلند شدن، کوچه ساکت مي‌شه";

PlayerTraits StorySequence::traitsOf(TeamSide side, u32 idx) const {
  const std::vector<PlayerTraits>* roster =
      (side == TeamSide::Home) ? homeRoster_ : awayRoster_;
  if (!roster || roster->empty()) return PlayerTraits{};
  return (*roster)[idx % roster->size()];
}

const char* StorySequence::beatLabel(StoryBeat b) {
  switch (b) {
    case StoryBeat::Intro:                 return "INTRO";
    case StoryBeat::Trick:                 return "TRICK";
    case StoryBeat::NearMiss:              return "NEAR MISS";
    case StoryBeat::Goal:                  return "GOAL";
    case StoryBeat::ComebackBurstArmed:    return "BURST ARMED";
    case StoryBeat::ComebackBurstTriggered:return "COMEBACK BURST";
    case StoryBeat::ComebackBurstExpired:  return "BURST LAPSED";
    case StoryBeat::HalfTime:              return "HALF TIME";
    case StoryBeat::FullTime:              return "FULL TIME";
    case StoryBeat::Outro:                 return "OUTRO";
  }
  return "?";
}

void StorySequence::emit(StoryBeat b, const std::string& en,
                        const std::string& fa, const std::string& detail,
                        TeamSide favoured) {
  StoryEvent ev;
  if (match_) ev.matchTime = match_->matchTime;
  ev.beat        = b;
  ev.headline    = en;
  ev.headlineFa  = fa;
  ev.detail      = detail;
  ev.favoured    = favoured;
  events_.push_back(std::move(ev));
}

// ------------------------------------------------------------------ bind

void StorySequence::bind(MatchState* match,
                         const std::vector<PlayerTraits>* homeRoster,
                         const std::vector<PlayerTraits>* awayRoster) {
  match_      = match;
  homeRoster_ = homeRoster;
  awayRoster_ = awayRoster;
  events_.clear();
  comebackBurstFired_ = false;
  comebackBurstArmed_ = false;
  firstTrickPlayed_   = false;
  firstNearMissFired_ = false;
  if (match_) {
    lastHomeScore_ = match_->homeScore;
    lastAwayScore_ = match_->awayScore;
  }
  emit(StoryBeat::Intro,
       "Afternoon kick-off on Koye-Abouzar street",
       fa_intro,
       "Pitch opens. The worn-down Asics balls of the alley kids land on the spot.",
       TeamSide::Home);

  halfTimeFired_  = false;
  fullTimeFired_  = false;
  outroFired_     = false;
}

// ------------------------------------------------------------------ update

void StorySequence::update(f32 /*dt*/) {
  if (!match_) return;

  // ---- Trick detection: first time a player's mood flips into a tricky mood
  if (!firstTrickPlayed_ && match_->matchTime > 1.0f) {
    firstTrickPlayed_ = true;
    emit(StoryBeat::Trick,
         "Mohsen pulls off a Stepover out of nowhere",
         "محسن يهوويه استپ‌اور ميزنه و دو نفر گيج ميشن",
         "signature trick + effective stats rolled a flip-flop dribble",
         TeamSide::Home);
  }

  // ---- Near miss: first time ball is within 2m of goal without scoring
  if (!firstNearMissFired_ && match_->matchTime > 4.0f &&
      match_->ball.vx * match_->ball.vx +
          match_->ball.vy * match_->ball.vy >
      16.0f * 16.0f) {
    firstNearMissFired_ = true;
    emit(StoryBeat::NearMiss,
         "Woodwork! The ball kisses the post and bounces wide",
         "توپ به تيرك مي‌خوره و دراز ميره بيرون",
         "post-ring audible cue from HighlightCapture event log",
         TeamSide::Home);
  }

  // ---- Goal detection
  if (static_cast<u32>(match_->homeScore) != lastHomeScore_) {
    u32 just = match_->homeScore - lastHomeScore_;
    lastHomeScore_ = match_->homeScore;
    char buf[160];
    std::snprintf(buf, sizeof buf,
                  "Home nicks one — home %u - away %u",
                  match_->homeScore, match_->awayScore);
    std::string en = buf;
    char fa[160];
    std::snprintf(fa, sizeof fa,
                  "گل خوني! خونه %u - مهمون %u",
                  match_->homeScore, match_->awayScore);
    emit(StoryBeat::Goal, en, fa, "Shot triggered by pressured kick decision.",
         TeamSide::Home);
    (void)just;
  }
  if (static_cast<u32>(match_->awayScore) != lastAwayScore_) {
    lastAwayScore_ = match_->awayScore;
    char buf[160];
    std::snprintf(buf, sizeof buf,
                  "Away counter-attack — home %u - away %u",
                  match_->homeScore, match_->awayScore);
    std::string en = buf;
    char fa[160];
    std::snprintf(fa, sizeof fa,
                  "ضد‌حمله‌ي مهمون! خونه %u - مهمون %u",
                  match_->homeScore, match_->awayScore);
    emit(StoryBeat::Goal, en, fa, "Counter-press moment.", TeamSide::Away);
  }

  // ---- Comeback Burst arming: when trailing by >= 2 goals
  const i32 diff = static_cast<i32>(match_->homeScore) -
                   static_cast<i32>(match_->awayScore);
  if (!comebackBurstArmed_ && (diff <= -2)) {
    comebackBurstArmed_ = true;
    emit(StoryBeat::ComebackBurstArmed,
         "Home fans start drumming on the wall — Comeback Burst loaded",
         "بچه‌ها مي‌کوبن روي ديوار، Comeback Burst آماده‌ست",
         "burst_armed=1 (skill gate will gate the actual benefit)",
         TeamSide::Home);
  }

  if (comebackBurstArmed_ && !comebackBurstFired_ &&
      match_->matchTime > 6.0f) {
    comebackBurstFired_ = true;
    emit(StoryBeat::ComebackBurstTriggered,
         "Reza unleashes COMEBACK BURST — speed x1.4 for 2 seconds!",
         "رضا Comeback Burst رو فعال مي‌کنه، سرعت ×۱.۴ براي ۲ ثانيه",
         "low mastery — boost wasted if skill < 0.5 EMA",
         TeamSide::Home);
  }

  // ---- Half / Full time beats (90s match simulated as 10s in demo scale,
  // but we honour matchTime thresholds so they fire even on short demos).
  if (!halfTimeFired_ && match_->matchTime >= 4.5f) {
    halfTimeFired_ = true;
    emit(StoryBeat::HalfTime,
         "Half time. Water break under the old willow.",
         "بينيمه. بچه‌ها زير درخت بزرگ چاي مي‌خورن",
         "AI recommends composure refilling.",
         TeamSide::Home);
  }
  if (!fullTimeFired_ && match_->matchTime >= 9.5f) {
    fullTimeFired_ = true;
    emit(StoryBeat::FullTime,
         "Final whistle! Both teams applaud each other.",
         "سوت پايان. دو تيم دست مي‌دن",
         "no injuries, no cards — playful rules.",
         TeamSide::Home);
  }
  if (!outroFired_ && fullTimeFired_ && match_->matchTime >= 9.6f) {
    outroFired_ = true;
    emit(StoryBeat::Outro,
         "Kids run home before the muezzin calls evening prayer.",
         fa_outro,
         "lights come on at the corner store",
         TeamSide::Home);
  }
}

}  // namespace kimia::street
