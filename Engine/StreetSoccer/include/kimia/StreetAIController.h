#pragma once
// =============================================================================
//  Street AI Controller
//
//  Lightweight behaviour-tree-ish controller for AI players. The controller
//  consumes PlayerTraits + Ball position + MatchState and produces a desired
//  velocity each tick. The physics bridge then applies it.
//
//  Behaviors (priority high -> low):
//    1. Score: when in opponent's third with the ball, drive to goal
//    2. Pass: when teammate is open and opponent is close, pass
//    3. Dribble: when in own/mid third, dribble toward opponent goal
//    4. Defend: when opponent has ball, intercept ball-carrier
//    5. Return: when too far from own goal, return to position
//    6. Idle: stand at home position
//
//  The controller also fires signature tricks via shouldAttemptTrick().
// =============================================================================

#include <kimia/StreetSoccer.h>
#include <kimia/StreetSoccerPhysics.h>
#include <kimia/PlayerTraits.h>
#include <kimia/Types.h>

namespace kimia::street {

struct AIDecision {
  f32 desiredVx = 0;
  f32 desiredVy = 0;
  bool wantsKick = false;
  f32 kickPower = 0;       // applied impulse magnitude
  bool wantsTrick = false;
};

// Build default traits for a team: assigns brazilianFlairTraits or
// iranianGritTraits based on team side.
PlayerTraits defaultTraitsForSide(TeamSide side, u8 shirtNumber);

// Update one AI player's decision based on current state.
AIDecision tickAI(const MatchState& state,
                  const Player& self,
                  const PlayerTraits& selfTraits,
                  i32 selfIndex,
                  TeamSide selfSide,
                  f32 dt);

// Tick the whole match (teams, ball, physics, AI).
// scoreDiff_out optional; set to (homeScore - awayScore) when match ends.
void tickMatch(MatchState& state,
               StreetPhysicsBridge& bridge,
               f32 dt,
               i32* scoreDiffOut = nullptr);

// Run AI for a player who has the ball — returns kick decision.
// Used by tickMatch when invoking AI on the ball carrier.
AIDecision kickDecision(const Player& carrier,
                        const PlayerTraits& carrierTraits,
                        const MatchState& state,
                        f32 dt);

}  // namespace kimia::street
