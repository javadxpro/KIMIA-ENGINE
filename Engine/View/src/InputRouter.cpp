#include <kimia/InputRouter.h>

#include <cmath>
#include <utility>

namespace kimia {

namespace {

// Every letter key is reported to the logic by name, so a rule saying "when
// key k" works with no engine change, and a component wired to "k" fires at
// the same moment.
const std::pair<Key, const char*> kNamedKeys[] = {
    {Key::A, "a"},     {Key::B, "b"},     {Key::C, "c"},     {Key::D, "d"},     {Key::E, "e"},
    {Key::F, "f"},     {Key::G, "g"},     {Key::H, "h"},     {Key::I, "i"},     {Key::J, "j"},
    {Key::K, "k"},     {Key::L, "l"},     {Key::M, "m"},     {Key::N, "n"},     {Key::O, "o"},
    {Key::P, "p"},     {Key::Q, "q"},     {Key::R, "r"},     {Key::S, "s"},     {Key::T, "t"},
    {Key::U, "u"},     {Key::V, "v"},     {Key::W, "w"},     {Key::X, "x"},     {Key::Y, "y"},
    {Key::Z, "z"},     {Key::Space, "space"}, {Key::Up, "up"}, {Key::Down, "down"},
    {Key::Left, "left"}, {Key::Right, "right"}, {Key::Return, "return"},
};

// Native controller buttons use the same action map as the WebWorkbench pads,
// so a rule listens for the game's own action rather than for hardware.
const std::pair<GamepadButton, const char*> kGamepadButtons[] = {
    {GamepadButton::A, "a"},               {GamepadButton::B, "b"},
    {GamepadButton::X, "x"},               {GamepadButton::Y, "y"},
    {GamepadButton::LeftShoulder, "l1"},   {GamepadButton::RightShoulder, "r1"},
    {GamepadButton::Back, "back"},         {GamepadButton::Start, "start"},
    {GamepadButton::DpadUp, "up"},         {GamepadButton::DpadDown, "down"},
    {GamepadButton::DpadLeft, "left"},     {GamepadButton::DpadRight, "right"},
};

}  // namespace

RoutedInput routeEditorInput(WorldEditor& editor, const InputState& input, f64 dt) {
  RoutedInput routed;

  // --- Menus and transport -------------------------------------------------
  // The option keys are fixed at Num1..Num9 because everything in this editor
  // is a numbered option; that is the whole interface.
  static const Key kOptionKeys[] = {Key::Num1, Key::Num2, Key::Num3, Key::Num4, Key::Num5,
                                    Key::Num6, Key::Num7, Key::Num8, Key::Num9};
  for (i32 i = 0; i < 9; ++i) {
    if (input.pressed(kOptionKeys[i])) editor.choose(i);
  }
  if (input.pressed(Key::R)) editor.resetBall();
  if (input.pressed(Key::B)) editor.backToMenu();
  // Transport (the Unity toolbar, on the keyboard): Return starts Play from
  // any editor screen, Tab pauses the sim, Backspace steps a frame.
  if (input.pressed(Key::Return) && editor.hasWorld() && !editor.playing()) editor.enterPlayMode();
  if (input.pressed(Key::Tab) && editor.playing()) editor.setPaused(!editor.paused());
  if (input.pressed(Key::Backspace) && editor.paused()) editor.stepOnce(1.0 / 60.0);

  // --- The one action button ----------------------------------------------
  // Mouse, pad A and (in shot mode) Space all mean "do the main thing": shoot
  // in golf, jump in football.
  const bool padAction = input.gamepadDown(GamepadButton::A);
  const bool padActionPressed = input.gamepadPressed(GamepadButton::A);
  const bool mouseAction = input.mouseDown(MouseButton::Left);
  const bool mouseActionPressed = input.mousePressed(MouseButton::Left);
  if (editor.shotMode()) {
    editor.setShootHeld(input.down(Key::Space) || mouseAction || padAction);
  } else {
    editor.setShootHeld(false);
    if (input.pressed(Key::J) || input.pressed(Key::Space) || mouseActionPressed || padActionPressed) {
      editor.jumpPressed();
    }
  }

  // --- Ball control --------------------------------------------------------
  // Hold C to dribble, hold Q/E to curl the next strike, tap P to pass.
  editor.setDribbleHeld(input.down(Key::C));
  f64 curl = 0.0;
  if (input.down(Key::Q)) curl -= 1.0;
  if (input.down(Key::E)) curl += 1.0;
  if (curl != 0.0) editor.setCurl(curl);
  if (input.pressed(Key::P)) editor.pass();

  // --- Named keys, the logic, and the input map ----------------------------
  for (const auto& binding : kNamedKeys) {
    if (input.pressed(binding.first)) {
      routed.pressedKeys.push_back(binding.second);
      editor.fireTrigger(binding.second);  // components wired to this key
    }
    if (input.down(binding.first)) routed.heldKeys.push_back(binding.second);
  }
  editor.setLogicKeys(routed.pressedKeys, routed.heldKeys);
  for (const std::string& key : routed.pressedKeys) {
    const std::string action = editor.actionFromControl(Source::Key, key);
    if (!action.empty()) {
      editor.fireControl(action);
      routed.actions.push_back(action);
    }
  }
  for (const auto& binding : kGamepadButtons) {
    if (!input.gamepadPressed(binding.first)) continue;
    const std::string action = editor.actionFromControl(Source::Pad, binding.second);
    if (!action.empty()) {
      editor.fireControl(action);
      routed.actions.push_back(action);
    }
  }

  // --- Arena: hold F to fire, tap R to reload ------------------------------
  // The football keys around it are harmless there — there is no ball to kick.
  if (editor.arenaMode()) {
    editor.setFireHeld(input.down(Key::F));
    if (input.pressed(Key::R)) editor.reload();
  }

  // --- Skill moves: taps, because you commit to them -----------------------
  if (input.pressed(Key::N)) editor.startTrick(WorldEditor::Trick::Nutmeg);
  if (input.pressed(Key::O)) editor.startTrick(WorldEditor::Trick::Roulette);
  if (input.pressed(Key::U)) editor.startTrick(WorldEditor::Trick::Juggle);

  // --- Movement ------------------------------------------------------------
  // Arrows and the left stick feed the same two numbers; the stick wins when
  // it is pushed further, which is what a player expects when both are used.
  if (input.down(Key::Left)) routed.moveX -= 1.0;
  if (input.down(Key::Right)) routed.moveX += 1.0;
  if (input.down(Key::Up)) routed.moveZ -= 1.0;
  if (input.down(Key::Down)) routed.moveZ += 1.0;
  const f64 stickX = input.gamepadAxis(GamepadAxis::LeftX);
  const f64 stickY = input.gamepadAxis(GamepadAxis::LeftY);
  if (std::abs(stickX) > std::abs(routed.moveX)) routed.moveX = stickX;
  if (std::abs(stickY) > std::abs(routed.moveZ)) routed.moveZ = stickY;
  editor.setMoveInput(routed.moveX, routed.moveZ);
  routed.fine = input.down(Key::Shift);
  editor.setFineMove(routed.fine);

  // --- Camera --------------------------------------------------------------
  const f64 padLookX = input.gamepadAxis(GamepadAxis::RightX) * 90.0;
  const f64 padLookY = input.gamepadAxis(GamepadAxis::RightY) * 70.0;
  if (editor.cameraControlled()) {
    // Orbit the camera with the arrows (and the mouse); the same pads drive
    // the ghost in placing and moving.
    routed.camera.yawDelta = (routed.moveX * 1.1 + input.lookX * 0.006 + padLookX) * dt;
    routed.camera.pitchDelta = (-routed.moveZ * 0.9 + input.lookY * 0.006 + padLookY) * dt;
    if (input.pressed(Key::Q)) routed.camera.zoom(1.0 / 1.2);
    if (input.pressed(Key::E)) routed.camera.zoom(1.2);
    if (input.zoom != 0.0) routed.camera.zoom(std::pow(1.2, -input.zoom));
    routed.camera.reset = input.pressed(Key::C);
    // An editor screen owns the arrows, so the distance it leaves behind is
    // the one the user chose by hand.
    routed.camera.handSet = true;
  } else {
    routed.camera.yawDelta = input.lookX * 0.006 + padLookX * dt;
    routed.camera.pitchDelta = input.lookY * 0.006 + padLookY * dt;
    if (input.zoom != 0.0) routed.camera.zoom(std::pow(1.2, -input.zoom));
  }
  return routed;
}

}  // namespace kimia
