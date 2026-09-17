#include <kimia/HighlightCapture.h>

namespace kimia::street {

HighlightCapture::HighlightCapture() = default;

void HighlightCapture::setWindow(f32 before, f32 after) {
  before_ = before;
  after_  = after;
}

bool HighlightCapture::detectGoal(const MatchState& before,
                                  const MatchState& after,
                                  TeamSide& sideOut) {
  if (after.homeScore > before.homeScore) {
    sideOut = TeamSide::Home;
    return true;
  }
  if (after.awayScore > before.awayScore) {
    sideOut = TeamSide::Away;
    return true;
  }
  return false;
}

Highlight HighlightCapture::makeGoalHighlight(TeamSide side,
                                              const MatchState& state,
                                              const std::string& scorer,
                                              f32 time) {
  Highlight h;
  h.kind = HighlightKind::Goal;
  h.startTime = time - 5.0f;
  h.endTime   = time + 5.0f;
  h.homeScore = state.homeScore;
  h.awayScore = state.awayScore;
  h.scorer = scorer;
  return h;
}

void HighlightCapture::tick(const MatchState& state, f32 time) {
  if (first_) {
    lastHomeScore_ = state.homeScore;
    lastAwayScore_ = state.awayScore;
    first_ = false;
    return;
  }
  if (state.homeScore != lastHomeScore_ ||
      state.awayScore != lastAwayScore_) {
    Highlight h;
    h.kind = HighlightKind::Goal;
    h.startTime = time - before_;
    h.endTime   = time + after_;
    h.homeScore = state.homeScore;
    h.awayScore = state.awayScore;
    // Find the most recent goal to attribute a scorer.
    if (!state.goals.empty()) {
      const auto& last = state.goals.back();
      h.scorer = (last.scoredBy == TeamSide::Home) ? "Home" : "Away";
      // Look up the scorer name from the matching team.
      const auto& team = (last.scoredBy == TeamSide::Home)
          ? state.homeTeam : state.awayTeam;
      for (const auto& p : team) {
        if (p.hasBall) { h.scorer = p.name; break; }
      }
    }
    stored_.push_back(h);
    lastHomeScore_ = state.homeScore;
    lastAwayScore_ = state.awayScore;
  }
}

}  // namespace kimia::street
