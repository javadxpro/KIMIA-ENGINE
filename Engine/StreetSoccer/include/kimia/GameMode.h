#pragma once
// =============================================================================
//  GameMode — selector + per-mode tuning.
//
//  Required by phase 1: "حالت های بازی" + "حالت رایگان اولیه".
//
//  4 modes:
//    Street : default, asphalt + concrete walls, normal gravity,
//             hard rebounds, balanced physics.
//    Comedy : zero gravity on the X axis, ball bounce doubled (palest
//             trick "bounce" springs), crowd reaction sound cue.
//    Grass  : grass pitch (Spain vibe), higher friction, lower bounce,
//             no-Comback-Burst nerf (real grass feels real).
//    Speed  : fixedDt halved (we tick at 120Hz), ball lighter, more
//             trick success, sprint x1.5.
//
//  Free trial mode is the default: full Street at 60s matches, no
//  purchase gates. Players can earn Coins but not Star/Gem/Token
//  unless they sign in (handled by the surrounding tournament plugin).
// =============================================================================

#include <kimia/Types.h>

namespace kimia::street {

enum class GameMode : u8 {
  Street = 0,    // default, free trial
  Comedy = 1,    // chaos / laugh-only
  Grass  = 2,    // realistic grass pitch
  Speed  = 3,    // arcade / fast
};

struct ModeTuning {
  f32 gravityScale       = 1.0f;  // 1.0 normal; 0.55 Comedy, 1.0 Grass, 0.85 Speed
  f32 ballRestitution    = 0.40f;
  f32 ballFriction       = 0.40f;
  f32 sprintMultiplier   = 1.0f;  // 1.0 Street, 1.0 Grass, 1.5 Speed, 1.2 Comedy
  f32 trickChanceBoost   = 0.0f;  // 0 Street, 0.2 Comedy, 0 Speed, -0.05 Grass
  bool comebackBurstAllowed = true;
  f32 fixedDt            = 1.0f/60.0f;
  i32 halfTimeSeconds    = 180;   // 3 minutes per half (street rules)
};

ModeTuning tuningFor(GameMode m);
const char* modeLabel(GameMode m);
const char* modeLabelFa(GameMode m);

}  // namespace kimia::street
