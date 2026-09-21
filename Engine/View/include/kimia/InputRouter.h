#pragma once

#include <kimia/CameraController.h>
#include <kimia/GameplayEvents.h>
#include <kimia/InputState.h>
#include <kimia/Types.h>
#include <kimia/World.h>

#include <string>
#include <vector>

namespace kimia {

// What one frame of raw input asked the world to do, in the order it was
// asked. Returned so a caller (or a test) can see the routing result without
// re-deriving it from the world's state.
struct RoutedInput {
  // Everything the camera needs this frame. Hand it to CameraController.
  CameraInput camera;
  // Movement intent, -1..1 per axis, already including the controller stick.
  f64 moveX = 0.0;
  f64 moveZ = 0.0;
  bool fine = false;
  // Named keys held/pressed this frame: what the visual logic listens for.
  std::vector<std::string> pressedKeys;
  std::vector<std::string> heldKeys;
  // Input-map actions this frame's presses fired (by action name).
  std::vector<std::string> actions;
};

// Routes one frame of raw input into the editor: menu picks, transport,
// shot/dribble/curl/pass, the input map, the arena trigger, the skill moves,
// movement, and the camera contract.
//
// This block used to be ~90 lines in the middle of the frame loop, where the
// only way to check a key binding was to press it. As a function over
// (editor, input, dt) it is testable: a test latches keys on an InputState and
// asserts on what the world did.
//
// Call it while holding whatever lock protects the editor: it mutates it.
RoutedInput routeEditorInput(WorldEditor& editor, const InputState& input, f64 dt);

}  // namespace kimia
