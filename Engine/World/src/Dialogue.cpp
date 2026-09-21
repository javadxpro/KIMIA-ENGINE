// Spoken lines: the component, the trigger that fires it, and the caption.
//
// Split out of World.cpp with no behaviour change. A DialogueComponent is data
// on an entity (Engine/Scene/src/SceneIO.cpp writes it as `say`); this file is
// the runtime half — which line is showing right now, for how long, and which
// gameplay event or key press wakes the next one.
#include "WorldInternal.h"

#include <kimia/World.h>

#include <algorithm>
#include <cmath>

namespace kimia {

using namespace worldinternal;  // the World module's own toolbox (see the header)


void WorldEditor::showDialogue(const std::string& speaker, DialogueComponent line) {
  if (line.line.empty() || line.holdSeconds <= 0.0) return;
  // One line per speaker: a second line from the same person replaces the
  // first (people do not talk over themselves), while another speaker's line
  // is simply also on screen.
  for (ActiveDialogue& active : dialogue_) {
    if (active.speaker != speaker) continue;
    active.component = std::move(line);
    active.remaining = active.component.holdSeconds;
    return;
  }
  ActiveDialogue active;
  active.speaker = speaker;
  active.component = std::move(line);
  active.remaining = active.component.holdSeconds;
  dialogue_.push_back(std::move(active));
}

void WorldEditor::updateDialogue(f64 dt) {
  if (dialogue_.empty() || dt <= 0.0) return;
  for (usize i = dialogue_.size(); i > 0U; --i) {
    dialogue_[i - 1U].remaining -= dt;
    if (dialogue_[i - 1U].remaining <= 0.0) {
      dialogue_.erase(dialogue_.begin() + static_cast<std::ptrdiff_t>(i - 1U));
    }
  }
}

void WorldEditor::clearDialogue() { dialogue_.clear(); }

std::vector<std::string> WorldEditor::dialogueLines() const {
  std::vector<std::string> lines;
  lines.reserve(dialogue_.size());
  for (const ActiveDialogue& active : dialogue_) {
    lines.push_back(active.speaker.empty() ? active.component.line
                                           : active.speaker + ": " + active.component.line);
  }
  return lines;
}

usize WorldEditor::fireDialogue(const std::string& entityName, const std::string& trigger) {
  EntityData* entity = world_.scene.get(world_.scene.find(entityName));
  if (entity == nullptr || entity->dialogue.empty()) return 0U;
  usize started = 0U;
  const std::string speaker = entity->name;
  for (const DialogueComponent& line : entity->dialogue) {
    if (line.trigger != trigger) continue;
    showDialogue(speaker, line);
    ++started;
  }
  return started;
}

usize WorldEditor::fireDialogueTrigger(const std::string& trigger) {
  if (trigger.empty()) return 0U;
  // Collected first, then shown. Nothing here modifies the scene today, but a
  // container must not be mutated while it is walked — and the next person to
  // touch this should not have to re-derive whether it is safe.
  std::vector<std::pair<std::string, DialogueComponent>> firing;
  world_.scene.forEach([&firing, &trigger](EntityHandle, const EntityData& entity) {
    for (const DialogueComponent& line : entity.dialogue) {
      if (line.trigger == trigger) firing.emplace_back(entity.name, line);
    }
  });
  for (const auto& entry : firing) showDialogue(entry.first, entry.second);
  return firing.size();
}

}  // namespace kimia
