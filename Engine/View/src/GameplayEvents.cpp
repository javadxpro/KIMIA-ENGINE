#include <kimia/GameplayEvents.h>

#include <kimia/Audio.h>

namespace kimia {

namespace {

// The one list. Order matches the enum in World.h, and the unit test walks
// every value so a new event with no cue fails the build's test run rather
// than going silent in the game.
const GameplayEventInfo kEvents[] = {
    {WorldEditor::GameEvent::Shot, "shot", "shot"},
    {WorldEditor::GameEvent::Kick, "kick", "kick"},
    {WorldEditor::GameEvent::Holed, "holed", "holed"},
    {WorldEditor::GameEvent::Goal, "goal", "goal"},
    {WorldEditor::GameEvent::RoundOver, "roundover", "round"},
    {WorldEditor::GameEvent::Whistle, "whistle", "whistle"},
    {WorldEditor::GameEvent::Tackle, "tackle", "tackle"},
    {WorldEditor::GameEvent::Trick, "trick", "trick"},
};

const GameplayEventInfo* infoFor(WorldEditor::GameEvent event) {
  for (const GameplayEventInfo& info : kEvents) {
    if (info.event == event) return &info;
  }
  return nullptr;
}

}  // namespace

const std::vector<GameplayEventInfo>& gameplayEvents() {
  static const std::vector<GameplayEventInfo> table(std::begin(kEvents), std::end(kEvents));
  return table;
}

const char* soundCueFor(WorldEditor::GameEvent event) {
  const GameplayEventInfo* info = infoFor(event);
  return info != nullptr ? info->cue : "";
}

const char* triggerFor(WorldEditor::GameEvent event) {
  const GameplayEventInfo* info = infoFor(event);
  return info != nullptr ? info->trigger : "";
}

std::vector<const char*> pumpGameplayEvents(WorldEditor& editor) {
  std::vector<const char*> cues;
  for (const WorldEditor::GameEvent event : editor.drainEvents()) {
    const char* trigger = triggerFor(event);
    // A component bound to this trigger fires here, without any game code
    // knowing the component exists. An empty trigger is never fired: an
    // unbound "" would match every component with an empty trigger field.
    if (trigger != nullptr && trigger[0] != '\0') editor.fireTrigger(trigger);
    const char* cue = soundCueFor(event);
    if (cue != nullptr && cue[0] != '\0') cues.push_back(cue);
  }
  return cues;
}

std::vector<std::pair<std::string, std::vector<u8>>> gameplaySoundBank() {
  std::vector<std::pair<std::string, std::vector<u8>>> bank;
  // Procedural cues: no asset files, so a published game has sound with
  // nothing next to the binary.
  bank.emplace_back("shot", AudioBuffer::thock(0.12, 1400.0).encodeWAV());
  bank.emplace_back("kick", AudioBuffer::thock(0.16, 700.0).encodeWAV());
  bank.emplace_back("holed",
                    AudioBuffer::concat(AudioBuffer::tone(660.0, 0.12), AudioBuffer::tone(990.0, 0.25)).encodeWAV());
  bank.emplace_back("goal", AudioBuffer::tone(440.0, 0.5, 0.6, 880.0).encodeWAV());
  bank.emplace_back("round",
                    AudioBuffer::concat(AudioBuffer::concat(AudioBuffer::tone(523.25, 0.15),
                                                            AudioBuffer::tone(659.25, 0.15)),
                                        AudioBuffer::tone(783.99, 0.35))
                        .encodeWAV());
  // A referee's whistle is a shrill held note; a tackle is a duller, lower
  // thock than a clean kick; a completed trick is a bright rising flourish.
  bank.emplace_back("whistle", AudioBuffer::tone(2100.0, 0.30, 0.5, 2400.0).encodeWAV());
  bank.emplace_back("tackle", AudioBuffer::thock(0.20, 320.0).encodeWAV());
  bank.emplace_back("trick",
                    AudioBuffer::concat(AudioBuffer::tone(880.0, 0.09), AudioBuffer::tone(1318.5, 0.16)).encodeWAV());
  return bank;
}

}  // namespace kimia
