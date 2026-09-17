#pragma once
// =============================================================================
//  DialogueSystem — street-tone one-liners.
//  فاز ۱: "سیستم دیالوگ های خنده‌دار" + "درجه سختی با ادبیات خیابونی"
//
//  Tone is highly localised Persian — kids in Koye-Abouzar trash-talk
//  each other in a specific cadence. We seed a small table of one-liners
//  per event and at random points in the match.
//
//  Difficulty levels shape the taunt:
//    Easy   — friendly ("بچه ها، بریم بازی!")
//    Medium — spirited trash
//    Hard   — full-on alley trash talk
//
//  Comedy mode deliberately swaps some lines for nonsensical rhyme, so
//  the same engine reads both serious and goofy.
// =============================================================================

#include <kimia/GameMode.h>
#include <kimia/Types.h>
#include <string>
#include <vector>

namespace kimia::street {

enum class DialogueLevel : u8 {
  Easy   = 0,
  Medium = 1,
  Hard   = 2,
};

enum class DialogueTrigger : u8 {
  OnKickOff,
  OnGoal,
  OnNearMiss,
  OnTrick,
  OnComebackArmed,
  OnComebackFired,
  OnHalfTime,
  OnFullTime,
  OnMidIdle,
};

struct DialogueLine {
  DialogueTrigger trigger;
  DialogueLevel   level;
  std::string     textFa;
  std::string     textEn;
  GameMode        mode = GameMode::Street;  // Street or Comedy filter
};

// Big static table (single source of truth). Function picks lines.
class DialogueSystem {
public:
  DialogueSystem();

  // Pull a line for a trigger, taking into account level + mode.
  DialogueLine pull(DialogueTrigger t, DialogueLevel lvl,
                     GameMode mode,
                     u32* rngState) const;

  // Force comedy for any line.
  std::vector<DialogueLine> all() const { return lines_; }

private:
  std::vector<DialogueLine> lines_;
};

// ASCII renderer for the dialogue. Pure stdio, ANSI-tolerant.
void renderDialogue(const DialogueLine& line);

}  // namespace kimia::street
