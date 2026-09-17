// TermuxConsole.cpp — headless ASCII+ANSI renderer.

#include <kimia/TermuxConsole.h>
#include <kimia/PlayerTraits.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace kimia::street {

namespace {

// cheap colour detection: most modern terminals handle ANSI. We don't
// gate by TERM env because Termux always supports it.
[[maybe_unused]] constexpr const char* C_RED    = "\x1b[31m";
constexpr const char* C_GREEN  = "\x1b[32m";
constexpr const char* C_YELLOW = "\x1b[33m";
[[maybe_unused]] constexpr const char* C_BLUE   = "\x1b[34m";
constexpr const char* C_MAGENTA = "\x1b[35m";
constexpr const char* C_CYAN   = "\x1b[36m";
constexpr const char* C_BOLD   = "\x1b[1m";
constexpr const char* C_DIM    = "\x1b[2m";
constexpr const char* C_RESET  = "\x1b[0m";

// Combined strings as literal concatenation of adjacent string literals.
static constexpr const char* CL_BOLD_RED    = "\x1b[1m\x1b[31m";
static constexpr const char* CL_DIM_RED     = "\x1b[2m\x1b[31m";
static constexpr const char* CL_DIM_CYAN    = "\x1b[2m\x1b[36m";
static constexpr const char* CL_BOLD_GREEN  = "\x1b[1m\x1b[32m";
static constexpr const char* CL_BOLD_YELLOW = "\x1b[1m\x1b[33m";

constexpr const char* pickColourFor(StoryBeat b) {
  switch (b) {
    case StoryBeat::Intro:
    case StoryBeat::Outro:
    case StoryBeat::HalfTime:
    case StoryBeat::FullTime:
      return C_CYAN;
    case StoryBeat::Trick:                return C_MAGENTA;
    case StoryBeat::NearMiss:             return C_YELLOW;
    case StoryBeat::Goal:                 return C_GREEN;
    case StoryBeat::ComebackBurstArmed:
    case StoryBeat::ComebackBurstTriggered: return CL_BOLD_RED;
    case StoryBeat::ComebackBurstExpired:
      return CL_DIM_RED;
  }
  return C_RESET;
}

}  // namespace

TermuxConsole::TermuxConsole() : coloured_(true) {}

std::string TermuxConsole::withColour(const std::string& s,
                                      const char* code) const {
  if (!coloured_) return s;
  std::string out = code;
  out += s;
  out += C_RESET;
  return out;
}

void TermuxConsole::clearScreen() const {
  std::fputs("\x1b[2J\x1b[H", stdout);
  std::fflush(stdout);
}

void TermuxConsole::moveTo(u32 row, u32 col) const {
  std::fprintf(stdout, "\x1b[%u;%uH", row, col);
}

void TermuxConsole::renderTopBanner(const char* text,
                                    const char* colour) const {
  std::fputs("\n", stdout);
  std::fputs(withColour("══════════════════════════════════════════", C_BOLD).c_str(),
            stdout);
  std::fputs("\n", stdout);
  std::fputs(withColour(text, colour).c_str(), stdout);
  std::fputs("\n", stdout);
  std::fputs(withColour("══════════════════════════════════════════", C_BOLD).c_str(),
            stdout);
  std::fputs("\n\n", stdout);
}

void TermuxConsole::renderCentreField() const {
  std::fputs(withColour("STREET SOCCER — KOYE-ABOUZAR — TERMUX", CL_BOLD_YELLOW).c_str(),
            stdout);
  std::fputs("\n", stdout);
}

// ------------------------------------------------------------------ main API

void TermuxConsole::playStory(const StorySequence& seq, const MatchState& match,
                              const std::vector<PlayerTraits>& /*homeRoster*/,
                              const std::vector<PlayerTraits>& /*awayRoster*/) {
  (void)match;
  std::vector<bool> fired(seq.events().size(), false);

  auto showEvent = [&](u32 i, const StoryEvent& ev, bool last) {
    if (fired[i]) return;
    fired[i] = true;

    char head[256];
    std::snprintf(head, sizeof head,
                  "▶ [t=%5.1fs] %s",
                  ev.matchTime, StorySequence::beatLabel(ev.beat));
    std::fputs(withColour(head, pickColourFor(ev.beat)).c_str(), stdout);
    std::fputs("\n", stdout);

    std::fputs("  ", stdout);
    std::fputs(withColour(ev.headline, C_BOLD).c_str(), stdout);
    std::fputs("\n", stdout);

    std::fputs("  ", stdout);
    std::fputs(withColour(ev.headlineFa, CL_DIM_CYAN).c_str(), stdout);
    std::fputs("\n", stdout);

    if (!ev.detail.empty()) {
      std::fputs("    ", stdout);
      std::fputs(withColour(ev.detail, C_DIM).c_str(), stdout);
      std::fputs("\n", stdout);
    }
    std::fputs("\n", stdout);

    if (last) {
      renderTopBanner("FULL TIME", CL_BOLD_GREEN);
      char score[128];
      std::snprintf(score, sizeof score,
                    "  %s %u  —  %u %s",
                    "Home", match.homeScore,
                    match.awayScore, "Away");
      std::fputs(withColour(score, CL_BOLD_YELLOW).c_str(), stdout);
      std::fputs("\n", stdout);

      if (seq.comebackBurstFired()) {
        std::fputs(
            withColour("  ⚡ COMEBACK BURST FIRED — boosted speed, not boosted luck.",
                       CL_BOLD_RED).c_str(),
            stdout);
        std::fputs("\n", stdout);
      } else {
        std::fputs(
            withColour("  ⚪ No Comeback Burst — game stayed close. Skill > RNG.",
                       C_DIM).c_str(),
            stdout);
        std::fputs("\n", stdout);
      }
    }
  };

  renderCentreField();
  for (u32 i = 0; i < seq.events().size(); ++i) {
    showEvent(i, seq.events()[i], i + 1 == seq.events().size());
    std::fflush(stdout);
  }
}

void TermuxConsole::renderPitch(const MatchState& m) const {
  const u32 W = 60, H = 14;
  std::vector<std::string> grid(H, std::string(W, ' '));

  // Border
  for (u32 x = 0; x < W; ++x) {
    grid[0][x]     = '-';
    grid[H - 1][x] = '-';
  }
  for (u32 y = 0; y < H; ++y) {
    grid[y][0]     = '|';
    grid[y][W - 1] = '|';
  }
  // Centre line
  for (u32 x = 0; x < W; ++x) grid[H / 2][x] = (x == 0 || x == W - 1) ? '+' : '-';
  for (u32 y = 0; y < H; ++y) grid[y][W / 2] = (y == 0 || y == H - 1) ? '+' : ':';

  // Ball
  const f32 halfW = m.pitch.length * 0.5f;
  const f32 halfH = m.pitch.width * 0.5f;
  u32 bx = static_cast<u32>(((m.ball.x + halfW) / (2 * halfW)) * (W - 3)) + 1;
  u32 by = static_cast<u32>(((m.ball.y + halfH) / (2 * halfH)) * (H - 3)) + 1;
  if (bx >= W - 1) bx = W - 2;
  if (by >= H - 1) by = H - 2;
  grid[by][bx] = '*';

  for (auto& line : grid) {
    std::fputs(line.c_str(), stdout);
    std::fputs("\n", stdout);
  }
  std::fflush(stdout);
}

}  // namespace kimia::street
