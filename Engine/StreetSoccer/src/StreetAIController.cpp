#include <kimia/StreetAIController.h>
#include <cmath>

namespace kimia::street {

PlayerTraits defaultTraitsForSide(TeamSide side, u8 shirtNumber) {
  return (side == TeamSide::Home)
      ? brazilianFlairTraits(shirtNumber)
      : iranianGritTraits(shirtNumber);
}

namespace {
inline f32 dist(f32 ax, f32 ay, f32 bx, f32 by) {
  const f32 dx = ax - bx;
  const f32 dy = ay - by;
  return std::sqrt(dx * dx + dy * dy);
}

// Direction toward target, scaled by maxSpeed.
void toward(f32 fromX, f32 fromY, f32 tx, f32 ty,
            f32 maxSpeed, f32& outVx, f32& outVy) {
  const f32 dx = tx - fromX;
  const f32 dy = ty - fromY;
  const f32 d  = std::sqrt(dx * dx + dy * dy);
  if (d <= 0.0001f) { outVx = 0; outVy = 0; return; }
  outVx = (dx / d) * maxSpeed;
  outVy = (dy / d) * maxSpeed;
}
}

AIDecision tickAI(const MatchState& state,
                  const Player& self,
                  const PlayerTraits& selfTraits,
                  i32 selfIndex,
                  TeamSide selfSide,
                  f32 dt) {
  AIDecision d;
  const EffectiveStats stats = effectiveStats(selfTraits);
  const f32 maxSpeed = self.speed * (1.0f + stats.speedBonus);

  // Determine which goal we're attacking.
  const f32 ownGoalX   = (selfSide == TeamSide::Home)
      ? -state.pitch.length * 0.5f
      :  state.pitch.length * 0.5f;
  const f32 targetX    = -ownGoalX;
  const f32 goalY      = 0.0f;

  // Home position: line up across the pitch.
  // Spread players across their half.
  const f32 homeLineX  = (selfSide == TeamSide::Home) ? -3.0f : 3.0f;
  const f32 homeY      = (static_cast<f32>(selfIndex) - 1.5f) * 4.0f;
  const f32 homeX      = homeLineX;

  // Ball carrier is the closest player to the ball.
  const auto& team = (selfSide == TeamSide::Home) ? state.homeTeam : state.awayTeam;
  Player carrier; carrier.positionX = state.ball.x; carrier.positionY = state.ball.y;
  f32 bestDist = 1e9f;
  for (const auto& p : team) {
    if (p.hasBall) { carrier = p; bestDist = 0; break; }
    const f32 dd = dist(p.positionX, p.positionY,
                        state.ball.x, state.ball.y);
    if (dd < bestDist) {
      bestDist = dd;
      carrier.positionX = p.positionX;
      carrier.positionY = p.positionY;
    }
  }
  const bool iHaveBall = (self.hasBall ||
      dist(self.positionX, self.positionY, state.ball.x, state.ball.y) < 1.5f);

  if (iHaveBall) {
    // Score / pass / dribble.
    const f32 ballInOpponentThird =
        (selfSide == TeamSide::Home)
            ? state.ball.x > state.pitch.length * 0.25f
            : state.ball.x < -state.pitch.length * 0.25f;

    if (ballInOpponentThird && stats.aggression > 0.55f) {
      // Drive to goal.
      toward(self.positionX, self.positionY, targetX, goalY,
             maxSpeed * 1.2f, d.desiredVx, d.desiredVy);
      d.wantsKick = true;
      d.kickPower = 18.0f + stats.shotAccuracyBonus * 10.0f;
    } else if (stats.skill > 0.6f &&
               dist(self.positionX, self.positionY, targetX, goalY) < 8.0f) {
      // Shoot from distance.
      toward(self.positionX, self.positionY, targetX, goalY,
             maxSpeed, d.desiredVx, d.desiredVy);
      d.wantsKick = true;
      d.kickPower = 25.0f;
    } else {
      // Dribble forward.
      toward(self.positionX, self.positionY, targetX, goalY,
             maxSpeed * 0.6f, d.desiredVx, d.desiredVy);
      // Maybe try a trick.
      PlayerTraits opp; opp.skill = 0.5f; opp.unpredictability = 0.0f;
      if (shouldAttemptTrick(selfTraits, opp, dt)) {
        d.wantsTrick = true;
      }
    }
  } else {
    // Defenders: return to home. Attackers: support.
    const f32 distToHome = dist(self.positionX, self.positionY, homeX, homeY);
    if (distToHome > 8.0f || (self.isGoalkeeper && distToHome > 3.0f)) {
      // Return home.
      toward(self.positionX, self.positionY, homeX, homeY,
             maxSpeed, d.desiredVx, d.desiredVy);
    } else if (stats.aggression > 0.6f &&
               std::abs(self.positionX - state.ball.x) < 5.0f) {
      // Chase the ball.
      toward(self.positionX, self.positionY,
             state.ball.x, state.ball.y,
             maxSpeed, d.desiredVx, d.desiredVy);
    } else {
      // Idle drift.
      d.desiredVx = 0;
      d.desiredVy = 0;
    }
  }
  return d;
}

AIDecision kickDecision(const Player& carrier,
                        const PlayerTraits& carrierTraits,
                        const MatchState& state,
                        f32 /*dt*/) {
  AIDecision d;
  const EffectiveStats stats = effectiveStats(carrierTraits);
  d.wantsKick = true;
  d.kickPower = 14.0f + stats.shotAccuracyBonus * 12.0f;
  // Direction: toward the opposite goal.
  const f32 targetX = (carrier.team == TeamSide::Home)
      ? state.pitch.length * 0.5f
      : -state.pitch.length * 0.5f;
  toward(carrier.positionX, carrier.positionY, targetX, 0,
         1.0f, d.desiredVx, d.desiredVy);
  return d;
}

void tickMatch(MatchState& state,
               StreetPhysicsBridge& bridge,
               f32 dt,
               i32* scoreDiffOut) {
  // Step physics.
  bridge.world->advance(static_cast<double>(dt));
  syncFromPhysics(bridge, state, dt);
  state.matchTime += dt;

  // Goal detection.
  std::vector<GoalEvent> newGoals;
  detectGoals(state, newGoals);
  for (const auto& g : newGoals) {
    state.goals.push_back(g);
    if (g.scoredBy == TeamSide::Home) state.homeScore++;
    else                               state.awayScore++;
    teleportBall(bridge, 0.0f, 0.0f);
  }

  if (scoreDiffOut) {
    *scoreDiffOut = state.homeScore - state.awayScore;
  }
}

}  // namespace kimia::street
