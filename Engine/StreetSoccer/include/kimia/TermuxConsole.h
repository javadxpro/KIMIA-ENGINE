#pragma once
// =============================================================================
//  TermuxConsole — headless ASCII+ANSI renderer for Termux / plain terminals.
//
//  Reads StorySequence events plus a live MatchState and prints the
//  storyline beat by beat. No GPU, no SDL, no X11. Pure stdio. Designed
//  for Termux on Poco X3 Pro and any Linux without a display.
//
//  API:
//    TermuxConsole tc;
//    tc.playStory(seq, match, homeRoster, awayRoster);
//
//  Internally it also scrolls a small ASCII pitch and overlays the ball
//  position with a single '*' so the user gets a sense of motion.
// =============================================================================

#include <kimia/Types.h>
#include <kimia/StorySequence.h>
#include <kimia/StreetSoccer.h>
#include <cstdio>
#include <string>
#include <vector>

namespace kimia::street {

class TermuxConsole {
public:
  TermuxConsole();

  // Render the timeline produced by a StorySequence. Streams events to
  // stdout, including coloured banners. Safe to call on Termux (ANSI
  // supported) or plain dumb pipe (colours degrade gracefully).
  void playStory(const StorySequence& seq, const MatchState& match,
                 const std::vector<PlayerTraits>& homeRoster,
                 const std::vector<PlayerTraits>& awayRoster);

  // Live render of a single tick — a 16x16 mini-pitch with the ball
  // position. Useful for debugging trajectory.
  void renderPitch(const MatchState& match) const;

void clearScreen() const;

private:
  bool coloured_;

  void moveTo(u32 row, u32 col) const;
  std::string withColour(const std::string& s, const char* code) const;
  void renderCentreField() const;
  void renderTopBanner(const char* text, const char* colour) const;
};

}  // namespace kimia::street
