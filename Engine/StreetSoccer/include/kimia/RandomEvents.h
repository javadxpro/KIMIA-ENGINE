#pragma once
// =============================================================================
//  RandomEvents — environmental chaos during a street match.
//
//  Required by phase 1: "اتفاقات رندوم محیطی".
//
//  Periodically, mid-match, an event triggers that briefly changes
//  the environment. Each event is described in plain English (and short
//  Persian) so the StorySequence plus TermuxConsole can render it.
//
//  Examples:
//    * "A stray dog runs across the pitch — ball chase loses 3s"
//    * "A bicycle basket drops on Home #4 — sprint x0.8 for 5s"
//    * "Streetlight flicks off — FOV drops momentarily"
//    * "Younger kid joins in — adds +1 player to home side for 20s"
//    * "Wind kicks dust — pitch friction up for 8s"
//    * "Muezzin — match paused 5s in honour of evening prayer"
// =============================================================================

#include <kimia/Types.h>
#include <string>
#include <vector>

namespace kimia::street {

enum class RandomEventKind : u8 {
  StrayDog,
  BasketDrop,
  Blackout,
  KidJoins,
  DustStorm,
  EveningPrayer,
  IceCreamTruck,
  NewspaperBlows,
  PigeonHerd,
  TabbyCat,
};

struct RandomEvent {
  RandomEventKind kind;
  std::string     headlineFa;
  std::string     headlineEn;
  f32             durationSeconds = 0;
  f32             frictionDelta   = 0;  // added to pitch friction
  f32             gravityDelta    = 0;  // scale factor multiply
  f32             pitchFovDelta   = 0;
  bool            pauseMatch      = false;
  i32             extraHomePlayers = 0;
  i32             extraAwayPlayers = 0;
};

// Returns a list of events scheduled during the match.
// `seed` is a u32 LCG state; the function updates it via a stable rule.
std::vector<RandomEvent> generateRandomEvents(u32 seed, f32 matchSeconds = 600.0f);

// Pretty label for printing.
const char* eventLabel(RandomEventKind k);
const char* eventLabelFa(RandomEventKind k);

}  // namespace kimia::street
