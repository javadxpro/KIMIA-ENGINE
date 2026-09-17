#pragma once
// =============================================================================
//  Highlight Capture (Brazil Football Street / فوتبال خیابونی ایران)
//
//  Detects "interesting moments" in a match (goals, near-misses, big skills)
//  and writes them out as small replays. Designed for viral social sharing:
//  a 10-second highlight around each goal, ready to upload.
//
//  A highlight is a slice of frames from a ReplayRecorder, plus a small
//  metadata header (event type, scoring player, time).
// =============================================================================

#include <kimia/ReplaySystem.h>
#include <kimia/StreetSoccer.h>
#include <string>

namespace kimia::street {

enum class HighlightKind {
  Goal,
  NearMiss,
  BigTrick,
  Comeback,
  BuzzerBeater,
};

struct Highlight {
  HighlightKind kind = HighlightKind::Goal;
  f32 startTime = 0.0f;
  f32 endTime   = 0.0f;
  i32 homeScore = 0;
  i32 awayScore = 0;
  std::string scorer;
  ReplayFrame startFrame;
  ReplayFrame endFrame;
};

// Watch the live match and emit highlights when interesting things happen.
// Caller wires it up next to the ReplayRecorder.
class HighlightCapture {
 public:
  HighlightCapture();

  // How many seconds around an event to capture (default 5s each side).
  void setWindow(f32 before, f32 after);

  // Call once per match frame.
  void tick(const MatchState& state, f32 time);

  // Read collected highlights.
  const std::vector<Highlight>& highlights() const { return stored_; }

  // Clear all stored highlights.
  void clear() { stored_.clear(); }

  // Detect a goal between two snapshots. Returns true if a goal happened.
  static bool detectGoal(const MatchState& before,
                         const MatchState& after,
                         TeamSide& sideOut);

  // Build a Highlight around a goal event.
  static Highlight makeGoalHighlight(TeamSide side,
                                     const MatchState& state,
                                     const std::string& scorer,
                                     f32 time);

 private:
  f32 before_ = 5.0f;
  f32 after_  = 5.0f;
  i32 lastHomeScore_ = 0;
  i32 lastAwayScore_ = 0;
  bool first_ = true;
  std::vector<Highlight> stored_;
};

}  // namespace kimia::street
