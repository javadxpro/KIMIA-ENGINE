#pragma once
// =============================================================================
//  StorySequence — narrative engine for Brazil Football Street
//
//  Each match has a multi-beat storyline:
//    - intro ("طلوع ظهر تو کوچه ابوذر...")
//    - trick moments
//    - goals (with shooter's name)
//    - comeback triggers (Comeback Burst)
//    - outro (final whistle)
//
//  In headless Termux mode we render StoryEvents as ANSI-coloured text. On
//  Android we'll plug a Scene backend later. Same data, two renderers.
// =============================================================================

#include <kimia/Types.h>
#include <kimia/StreetSoccer.h>
#include <kimia/PlayerTraits.h>
#include <string>
#include <vector>

namespace kimia::street {

enum class StoryBeat : u8 {
  Intro,
  Trick,
  NearMiss,
  Goal,
  ComebackBurstArmed,
  ComebackBurstTriggered,
  ComebackBurstExpired,
  HalfTime,
  FullTime,
  Outro,
};

struct StoryEvent {
  f32         matchTime   = 0;       // seconds since match start
  StoryBeat   beat        = StoryBeat::Intro;
  std::string headline;             // rendered top, English
  std::string headlineFa;           // rendered bottom, Persian (RTL)
  std::string detail;
  TeamSide    favoured    = TeamSide::Home;
};

class StorySequence {
public:
  // Hook into a MatchState. Pass the match and the most recent event ids so
  // we don't double-count goals.
  void bind(MatchState* match,
            const std::vector<PlayerTraits>* homeRoster,
            const std::vector<PlayerTraits>* awayRoster);

  // Call after every tickMatch. Detects milestones.
  void update(f32 dt);

  const std::vector<StoryEvent>& events() const { return events_; }

  // True once update has produced at least one ComebackBurstTriggered event.
  bool comebackBurstFired() const { return comebackBurstFired_; }

  // Locales
  static const char* beatLabel(StoryBeat b);

private:
  MatchState*  match_ = nullptr;
  const std::vector<PlayerTraits>* homeRoster_ = nullptr;
  const std::vector<PlayerTraits>* awayRoster_ = nullptr;

  std::vector<StoryEvent> events_;
  u32 lastHomeScore_ = 0;
  u32 lastAwayScore_ = 0;
  bool comebackBurstFired_ = false;
  bool comebackBurstArmed_ = false;
  bool firstTrickPlayed_   = false;
  bool firstNearMissFired_ = false;
  bool halfTimeFired_      = false;
  bool fullTimeFired_      = false;
  bool outroFired_         = false;

  PlayerTraits traitsOf(TeamSide side, u32 idx) const;
  void emit(StoryBeat b, const std::string& en, const std::string& fa,
            const std::string& detail = "", TeamSide favoured = TeamSide::Home);
};

}  // namespace kimia::street
