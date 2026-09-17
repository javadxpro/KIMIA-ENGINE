#pragma once
// =============================================================================
//  TermuxInput — raw stdin reader for Termux / Linux keyboard input.
//
//  Polls stdin without blocking the main loop. Maps arrow keys / WASD to
//  a small movement intent suitable for one of the home-team players.
// =============================================================================

#include <kimia/Types.h>

namespace kimia::street {

struct Intent {
  f32    desiredVx = 0;       // -1 .. +1
  f32    desiredVy = 0;       // -1 .. +1
  bool   wantsKick = false;
  bool   wantsTrick = false;
};

class TermuxInput {
public:
  // Drain keyboard buffer; produces the latest intent. Non-blocking.
  // If no key is pressed, returns zero intent.
  Intent pollOnce();

  // Switches stdin to raw-mode so we get each keypress at once without
  // waiting for enter. Designed for POSIX (Termux / Linux).
  void enterRawMode();
  void leaveRawMode();
};

}  // namespace kimia::street
