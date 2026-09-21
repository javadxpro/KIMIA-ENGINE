#pragma once

#include <kimia/Types.h>
#include <kimia/World.h>

#include <string>
#include <utility>
#include <vector>

namespace kimia {

// One gameplay event as the rest of the engine sees it: the enum, the trigger
// name a component listens for, and the sound cue the app plays.
//
// Kept as one table because the alternative is what used to happen — the enum
// in World.h, a `switch` for the trigger name in World.cpp, another `switch`
// for the sound cue in the example, and a third list of registered sounds.
// Adding an event and forgetting one of the three was silent.
struct GameplayEventInfo {
  WorldEditor::GameEvent event;
  const char* trigger;  // what a component needs to bind to ("goal", "kick")
  const char* cue;      // the sound the app plays for it
};

// All gameplay events, in enum order.
const std::vector<GameplayEventInfo>& gameplayEvents();
// The cue for one event; never null (the table is exhaustive — a unit test
// walks every enum value).
const char* soundCueFor(WorldEditor::GameEvent event);
// The trigger name for one event (WorldEditor::eventTriggerName with a
// guaranteed non-empty answer).
const char* triggerFor(WorldEditor::GameEvent event);

// Drains the editor's pending events and does what each one means: fires the
// component trigger bound to it (so an object wired to "goal" reacts with no
// code behind it) and returns the sound cues to play, in the order the events
// happened. The caller plays the cues it has.
std::vector<const char*> pumpGameplayEvents(WorldEditor& editor);

// The procedural sound bank: one entry per cue, so the app registers what the
// engine actually raises and cannot drift. The bytes are a small WAV.
std::vector<std::pair<std::string, std::vector<u8>>> gameplaySoundBank();

}  // namespace kimia
