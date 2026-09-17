// GameMode.cpp — per-mode tuning tables.

#include <kimia/GameMode.h>

namespace kimia::street {

ModeTuning tuningFor(GameMode m) {
  ModeTuning t;
  switch (m) {
    case GameMode::Street:
      break;
    case GameMode::Comedy:
      t.gravityScale       = 0.55f;
      t.ballRestitution    = 0.65f;
      t.sprintMultiplier   = 1.20f;
      t.trickChanceBoost   = 0.20f;
      t.comebackBurstAllowed = true;
      break;
    case GameMode::Grass:
      t.gravityScale       = 1.0f;
      t.ballRestitution    = 0.25f;
      t.ballFriction       = 0.65f;
      t.sprintMultiplier   = 1.0f;
      t.trickChanceBoost   = -0.05f;
      t.comebackBurstAllowed = true;
      break;
    case GameMode::Speed:
      t.gravityScale       = 0.85f;
      t.ballRestitution    = 0.55f;
      t.ballFriction       = 0.30f;
      t.sprintMultiplier   = 1.50f;
      t.trickChanceBoost   = 0.0f;
      t.fixedDt            = 1.0f / 120.0f;
      t.comebackBurstAllowed = true;
      break;
  }
  return t;
}

const char* modeLabel(GameMode m) {
  switch (m) {
    case GameMode::Street: return "STREET (Koye-Abouzar)";
    case GameMode::Comedy: return "COMEDY (Chaos Bounce)";
    case GameMode::Grass:  return "GRASS (Spanish Pitch)";
    case GameMode::Speed:  return "SPEED (Arcade)";
  }
  return "?";
}

const char* modeLabelFa(GameMode m) {
  switch (m) {
    case GameMode::Street: return "خيابوني - کوي ابوذر";
    case GameMode::Comedy: return "خنده‌دار - پرش‌هاي عجيب";
    case GameMode::Grass:  return "چمن - زمين اسپانيا";
    case GameMode::Speed:  return "سريع - آرکيد";
  }
  return "?";
}

}  // namespace kimia::street
