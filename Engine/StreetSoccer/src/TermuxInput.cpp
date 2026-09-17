// TermuxInput.cpp — POSIX raw stdin reader.

#include <kimia/TermuxInput.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <termios.h>
#include <unistd.h>

namespace kimia::street {

namespace {
struct termios g_original;
bool g_set = false;
}  // namespace

void TermuxInput::enterRawMode() {
  if (g_set) return;
  if (tcgetattr(STDIN_FILENO, &g_original) == 0) {
    struct termios raw = g_original;
    raw.c_lflag &= ~(tcflag_t)(ECHO | ICANON);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    g_set = true;
  }
}

void TermuxInput::leaveRawMode() {
  if (!g_set) return;
  tcsetattr(STDIN_FILENO, TCSANOW, &g_original);
  g_set = false;
}

Intent TermuxInput::pollOnce() {
  Intent out;
  if (!g_set) return out;

  // Drain a single byte (we treat arrows as ESC sequences).
  char c = 0;
  const ssize_t n = ::read(STDIN_FILENO, &c, 1);
  if (n <= 0) return out;

  // Arrow key sequences
  if (c == 0x1b) {
    char seq[2] = {0, 0};
    ::read(STDIN_FILENO, &seq[0], 1);
    ::read(STDIN_FILENO, &seq[1], 1);
    if (seq[0] == '[') {
      switch (seq[1]) {
        case 'A': out.desiredVy = +1.0f; return out;  // up
        case 'B': out.desiredVy = -1.0f; return out;  // down
        case 'C': out.desiredVx = +1.0f; return out;  // right
        case 'D': out.desiredVx = -1.0f; return out;  // left
      }
    }
    return out;
  }

  switch (c) {
    case 'w': case 'W': out.desiredVy  = +1.0f; return out;
    case 's': case 'S': out.desiredVy  = -1.0f; return out;
    case 'a': case 'A': out.desiredVx  = -1.0f; return out;
    case 'd': case 'D': out.desiredVx  = +1.0f; return out;
    case ' ':            out.wantsKick  = true;  return out;
    case 'k': case 'K':  out.wantsTrick = true;  return out;
    case 'q': case 'Q':  // quit is the only signal that breaks the loop
      std::fputs("\n[TermuxInput] Quitting.\n", stdout);
      std::fflush(stdout);
      std::exit(0);
  }
  return out;
}

}  // namespace kimia::street
