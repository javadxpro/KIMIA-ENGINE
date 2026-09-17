#pragma once
// =============================================================================
//  Kimia Engine — Street Soccer base module
//
//  Brazil Football Street (global)
//  فوتبال خیابونی ایران (Iran)
//
//  Minimal foundation: pitch dimensions, ball state, players, and a single
//  MatchState that owns everything. Designed for Android (Poco X3 Pro) and
//  Windows. Everything here is pure data + a tick() that advances physics.
//
//  This module deliberately has no rendering: a separate Renderer module
//  reads MatchState. The AI module will own player behavior.
// =============================================================================

#include <kimia/Types.h>
#include <string>
#include <vector>

namespace kimia::street {

// -----------------------------------------------------------------------------
// Pitch
// -----------------------------------------------------------------------------
struct Pitch {
  // Street pitch is small: half-court size. Coordinates are in meters.
  f32 length = 28.0f;  // along X
  f32 width  = 16.0f;  // along Z (depth into screen)
  f32 goalWidth = 3.0f;
  f32 goalDepth = 1.5f;

  // Per-side boundaries: -length/2 .. +length/2, -width/2 .. +width/2.
};

// -----------------------------------------------------------------------------
// Team
// -----------------------------------------------------------------------------
enum class TeamSide { Home, Away };

// -----------------------------------------------------------------------------
// Player
// -----------------------------------------------------------------------------
struct Player {
  std::string name;
  TeamSide    team   = TeamSide::Home;
  u8          shirtNumber = 0;
  f32         positionX = 0;   // pitch coords
  f32         positionY = 0;
  f32         velocityX = 0;
  f32         velocityY = 0;
  f32         speed     = 6.0f;       // m/s top speed
  f32         stamina   = 100.0f;     // 0..100
  bool        hasBall   = false;
  bool        isGoalkeeper = false;
  i32         bodyId    = -1;          // physics body id (assigned by world)
};

// -----------------------------------------------------------------------------
// Ball
// -----------------------------------------------------------------------------
struct Ball {
  f32 x = 0.0f;
  f32 y = 0.0f;
  f32 vx = 0.0f;
  f32 vy = 0.0f;
  f32 radius = 0.11f;          // standard size 5 football ~ 11cm
  f32 mass   = 0.43f;          // ~430g
  i32 bodyId = -1;             // physics body id
};

// -----------------------------------------------------------------------------
// Goal
// -----------------------------------------------------------------------------
struct GoalEvent {
  TeamSide scoredBy;
  f32      timeSeconds;
};

struct MatchState {
  Pitch pitch;
  std::vector<Player> homeTeam;
  std::vector<Player> awayTeam;
  Ball ball;
  i32  homeScore = 0;
  i32  awayScore = 0;
  f32  matchTime = 0.0f;          // seconds since kickoff
  f32  halfTime  = 180.0f;        // 3 minutes per half (street rules)
  bool playing   = false;
  std::vector<GoalEvent> goals;
};

// -----------------------------------------------------------------------------
// Free-function helpers (no class to keep it header-light).
// -----------------------------------------------------------------------------

// Reset everything to kickoff (teams at their halves, ball at center).
void resetMatch(MatchState& m);

// Move ball one frame with simple friction (until physics is wired).
void tickBall(Ball& b, f32 dt);

// Detect goals based on ball position.
void detectGoals(MatchState& m, std::vector<GoalEvent>& newGoals);

// Convenience builder for a standard 4v4 street match.
void buildDefaultTeams(MatchState& m);

}  // namespace kimia::street
// touch
// touch: re-trigger CI with new workflow
