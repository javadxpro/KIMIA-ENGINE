// Computer players: the AI that fills a team's shirts (stage 27).
//
// Split out of World.cpp with no behaviour change — same code, same order of
// arguments, only a different translation unit. What it owns:
//
//   * who goes for the ball (aiChaser) and who guards the net (aiKeeper),
//   * the roles a player can hold (aiRole) and where each one wants to stand,
//   * how two computer players keep out of each other's way (aiSeparation),
//   * the per-frame step that turns all of that into movement (updateAi).
//
// The design is one idea: only ONE player per side goes for the ball; everyone
// else holds a shape. Without that rule every character runs at the ball at
// once and a match becomes a scrum.
#include "WorldInternal.h"

#include <kimia/World.h>

#include <algorithm>
#include <cmath>
#include <map>

namespace kimia {

using namespace worldinternal;  // the World module's own toolbox (see the header)


u32 WorldEditor::aiChaser(u32 team) const {
  if (!aiActive()) return 0U;
  const SphereBody* ball = physics_.sphere(ballId_);
  if (ball == nullptr) return 0U;
  const u32 keeper = aiKeeper(team);
  u32 best = 0U;
  f64 bestDistance = 0.0;
  for (const u32 id : physics_.characterIds()) {
    // The human is never picked: the player chases their own ball.
    if (id == kPrimaryCharacter) continue;
    const CharacterBody* body = physics_.characterById(id);
    if (body == nullptr || body->team != team) continue;
    if (id == keeper) continue;  // the keeper minds the net, not the ball
    const f64 dx = ball->position.x - body->position.x;
    const f64 dz = ball->position.z - body->position.z;
    const f64 distance = std::sqrt(dx * dx + dz * dz);
    if (best == 0U || distance < bestDistance) {
      best = id;
      bestDistance = distance;
    }
  }
  return best;
}

u32 WorldEditor::aiKeeper(u32 team) const {
  if (!aiActive()) return 0U;
  // A side needs somebody to spare: with one outfield player, that player
  // goes for the ball rather than standing on the line.
  u32 count = 0U;
  for (const u32 id : physics_.characterIds()) {
    if (id == kPrimaryCharacter) continue;
    const CharacterBody* body = physics_.characterById(id);
    if (body != nullptr && body->team == team) ++count;
  }
  if (count < 2U) return 0U;

  // The keeper is whoever is deepest in their own half.
  const f64 ownGoalZ = -attackDirectionZ(team) * world_.halfLength();
  u32 best = 0U;
  f64 bestDistance = 0.0;
  for (const u32 id : physics_.characterIds()) {
    if (id == kPrimaryCharacter) continue;
    const CharacterBody* body = physics_.characterById(id);
    if (body == nullptr || body->team != team) continue;
    const f64 distance = std::abs(body->position.z - ownGoalZ);
    if (best == 0U || distance < bestDistance) {
      best = id;
      bestDistance = distance;
    }
  }
  return best;
}

const char* WorldEditor::aiRoleName(AiRole role) {
  switch (role) {
    case AiRole::Keeper: return "KEEPER";
    case AiRole::Attack: return "ATTACK";
    case AiRole::Defend: return "DEFEND";
    case AiRole::Support: return "SUPPORT";
    case AiRole::Idle: break;
  }
  return "IDLE";
}

// A side has the ball when its chaser is on it AND is closer to it than
// anybody from the other side. Possession has to be EXCLUSIVE: when both
// sides thought they had it, both attacked, each dragged the ball the
// opposite way, and the two of them stood there cancelling out forever.
bool WorldEditor::aiHasPossession(u32 team) const {
  const SphereBody* ball = physics_.sphere(ballId_);
  if (ball == nullptr) return false;
  const u32 chaser = aiChaser(team);
  if (chaser == 0U) return false;
  const CharacterBody* body = physics_.characterById(chaser);
  if (body == nullptr) return false;
  const f64 dx = ball->position.x - body->position.x;
  const f64 dz = ball->position.z - body->position.z;
  const f64 mine = std::sqrt(dx * dx + dz * dz);
  if (mine >= world_.ball.radius + kAiTackleReach + kAiApproachOffset) return false;

  // Anybody closer — including the human — takes it off us.
  for (const u32 other : physics_.characterIds()) {
    const CharacterBody* rival = physics_.characterById(other);
    if (rival == nullptr || rival->team == team) continue;
    const f64 rx = ball->position.x - rival->position.x;
    const f64 rz = ball->position.z - rival->position.z;
    if (std::sqrt(rx * rx + rz * rz) < mine) return false;
  }
  return true;
}

// The net this side is attacking. Team 1 shoots at the -Z goal, team 2 at
// the +Z one (matching scoringTeamForGoalZ).
Vec3 WorldEditor::aiGoalMouth(u32 team) const {
  const f64 forward = attackDirectionZ(team);
  // Default: the middle of the far goal line, in case no goal was built.
  Vec3 mouth{0.0, 0.0, forward * world_.halfLength()};
  f64 half = kWorldGoalMedium * 0.5;
  std::map<std::string, GoalGroup> goals;
  scanGoals(world_.scene, goals);
  for (const auto& entry : goals) {
    const GoalGroup& goal = entry.second;
    if (!goal.valid()) continue;
    // The one we are shooting AT is on their side of halfway.
    if (forward > 0.0 ? goal.z() <= 0.0 : goal.z() > 0.0) continue;
    mouth = Vec3{goal.x(), 0.0, goal.z()};
    half = goal.width() * 0.5;
    break;
  }
  // Aim at a POST, not the middle. The keeper stands on the centre of its
  // line, so a side that always shoots down the middle is saved every
  // time — which is exactly why one team could never score. Each side
  // favours a different corner so the two are not mirror images.
  const SphereBody* ball = physics_.sphere(ballId_);
  const f64 side = ball != nullptr && ball->position.x > mouth.x ? 1.0 : -1.0;
  mouth.x += side * half * kAiGoalCorner;
  return mouth;
}

const char* WorldEditor::aiActionName(AiAction action) {
  switch (action) {
    case AiAction::Shoot: return "shoot";
    case AiAction::Pass: return "pass";
    case AiAction::Carry: return "carry";
    case AiAction::None: break;
  }
  return "none";
}

// How easy the pass to `mateId` is to cut out. 1 = nobody within
// kAiPassLaneClear of the lane; 0 = an opponent is standing on it (or there is
// no lane at all). Distance is measured to the SEGMENT, not to its ends: a
// defender level with the carrier but in the way still blocks the ball.
f64 WorldEditor::aiPassLaneQuality(u32 mateId) const {
  const SphereBody* ball = physics_.sphere(ballId_);
  const CharacterBody* mate = physics_.characterById(mateId);
  if (ball == nullptr || mate == nullptr) return 0.0;
  // The lane starts at the CARRIER, not at the ball: a pass is played from the
  // foot, and measuring from the ball makes the first metre of every lane look
  // blocked by whoever is on the ball.
  const CharacterBody* self = physics_.characterById(aiChaser(mate->team));
  const Vec3 from = self != nullptr ? Vec3{self->position.x, 0.0, self->position.z}
                                    : Vec3{ball->position.x, 0.0, ball->position.z};
  const Vec3 to{mate->position.x, 0.0, mate->position.z};
  const Vec3 along{to.x - from.x, 0.0, to.z - from.z};
  const f64 length = along.length();
  if (length <= kMoveEpsilon) return 0.0;  // on top of each other: no lane
  f64 closest = kAiPassLaneClear;
  for (const u32 other : physics_.characterIds()) {
    const CharacterBody* rival = physics_.characterById(other);
    if (rival == nullptr || rival->team == mate->team) continue;
    const Vec3 offset{rival->position.x - from.x, 0.0, rival->position.z - from.z};
    f64 t = kimia::dot(offset, along) / (length * length);
    t = std::min(1.0, std::max(0.0, t));  // inside the lane only
    const Vec3 point{from.x + along.x * t, 0.0, from.z + along.z * t};
    const f64 gap = std::sqrt((rival->position.x - point.x) * (rival->position.x - point.x) +
                              (rival->position.z - point.z) * (rival->position.z - point.z));
    closest = std::min(closest, gap);
  }
  return std::min(1.0, closest / kAiPassLaneClear);
}

Vec3 WorldEditor::aiPassLeadTarget(u32 mateId, f64 seconds) const {
  const CharacterBody* mate = physics_.characterById(mateId);
  if (mate == nullptr) return Vec3{0.0, 0.0, 0.0};
  const f64 lead = std::max(0.0, seconds);
  return Vec3{mate->position.x + mate->velocity.x * lead, mate->position.y, mate->position.z + mate->velocity.z * lead};
}

WorldEditor::AiDecision WorldEditor::aiDecision(u32 id) const {
  AiDecision decision;
  const CharacterBody* body = physics_.characterById(id);
  const SphereBody* ball = physics_.sphere(ballId_);
  if (body == nullptr || ball == nullptr || !aiActive()) return decision;
  // Only the player ON the ball makes this choice. Everyone else is holding
  // shape (see aiTargetFor), and inventing an action for them would put a lie
  // in the debug view.
  if (aiRole(id) != AiRole::Attack) return decision;
  const f64 ballDistance = std::sqrt((ball->position.x - body->position.x) * (ball->position.x - body->position.x) +
                                     (ball->position.z - body->position.z) * (ball->position.z - body->position.z));
  if (ballDistance >= world_.ball.radius + kAiTackleReach + kAiApproachOffset) return decision;
  const u32 team = body->team;
  const f64 skill = world_.profile.aiSkill;
  const f64 forward = attackDirectionZ(team);
  const Vec3 mouth = aiGoalMouth(team);

  // --- Shooting ---
  // The goal is worth the most, but only from range: a shot from the halfway
  // line is a pass to the other keeper. The keeper standing in the right place
  // takes the value down, which is what makes the AI look for the other corner.
  const f64 shotDistance = std::sqrt((mouth.x - ball->position.x) * (mouth.x - ball->position.x) +
                                     (mouth.z - ball->position.z) * (mouth.z - ball->position.z));
  if (shotDistance <= kAiShootRange) {
    decision.shootScore = 1.0 - shotDistance / kAiShootRange * 0.6;
    const u32 keeper = aiKeeper(team == 1U ? 2U : 1U);
    const CharacterBody* goalie = physics_.characterById(keeper);
    if (goalie != nullptr) {
      const f64 keeperGap = std::sqrt((goalie->position.x - mouth.x) * (goalie->position.x - mouth.x) +
                                      (goalie->position.z - mouth.z) * (goalie->position.z - mouth.z));
      // A keeper sitting on the line where we are aiming takes a good part of
      // the value off the shot; one dragged out of position leaves it as good
      // as it was. The floor keeps a tap-in a shot even with the keeper home —
      // a striker two metres out shooting into the keeper is football, and
      // turning that into a square pass would look like the AI is scared.
      decision.shootScore *= 0.55 + 0.45 * std::min(1.0, keeperGap / kAiKeeperRange);
    }
  }

  // --- Passing ---
  // A team-mate is worth passing to when the ball would end up further up the
  // pitch, nobody is standing in the lane, and the team-mate is not himself
  // marked. Skill weights the lane: a weak side only plays the safe ball.
  u32 bestMate = 0U;
  for (const u32 other : physics_.characterIds()) {
    if (other == id || other == kPrimaryCharacter) continue;
    const CharacterBody* mate = physics_.characterById(other);
    if (mate == nullptr || mate->team != team) continue;
    // Never pass back to the keeper, and never to someone level with us.
    if (other == aiKeeper(team)) continue;
    const f64 gain = (mate->position.z - ball->position.z) * forward;
    if (gain < kAiPassMinGain) continue;
    // How much room the team-mate has: a pass into a crowd is a tackle waiting
    // to happen. Measured against the nearest opponent.
    f64 room = kAiPassLaneClear;
    for (const u32 rival : physics_.characterIds()) {
      const CharacterBody* opponent = physics_.characterById(rival);
      if (opponent == nullptr || opponent->team == team) continue;
      const f64 gap = std::sqrt((opponent->position.x - mate->position.x) * (opponent->position.x - mate->position.x) +
                                (opponent->position.z - mate->position.z) * (opponent->position.z - mate->position.z));
      room = std::min(room, gap);
    }
    const f64 freeSpace = std::min(1.0, room / kAiPassLaneClear);
    const f64 lane = aiPassLaneQuality(other);
    const f64 laneWeight = kAiPassSkillWeight + (1.0 - kAiPassSkillWeight) * skill;
    const f64 score = (1.0 - laneWeight + laneWeight * lane) * (0.5 + 0.5 * freeSpace) *
                      std::min(1.0, gain / (world_.halfLength() * 0.5));
    if (score > decision.passScore) {
      decision.passScore = score;
      bestMate = other;
    }
  }

  // --- Carry (the old dribble-at-goal) ---
  // Worth something, because standing still is worth nothing; but it is the
  // worst option when a shot or a real pass exists.
  decision.carryScore = kAiCarryScore;

  decision.action = AiAction::Carry;
  decision.target = mouth;
  decision.score = decision.carryScore;
  decision.runnerUp = 0.0;
  if (decision.passScore > decision.score) {
    decision.action = AiAction::Pass;
    decision.targetId = bestMate;
    const CharacterBody* mate = physics_.characterById(bestMate);
    if (mate != nullptr) decision.target = Vec3{mate->position.x, 0.0, mate->position.z};
    decision.score = decision.passScore;
    decision.runnerUp = std::max(decision.carryScore, decision.shootScore);
  } else {
    decision.runnerUp = decision.passScore;
  }
  if (decision.shootScore > decision.score) {
    decision.action = AiAction::Shoot;
    decision.target = mouth;
    decision.runnerUp = std::max(decision.score, decision.passScore);
    decision.score = decision.shootScore;
  }
  return decision;
}

WorldEditor::AiRole WorldEditor::aiRole(u32 id) const {
  if (!aiActive() || id == kPrimaryCharacter) return AiRole::Idle;
  const CharacterBody* body = physics_.characterById(id);
  if (body == nullptr) return AiRole::Idle;
  const u32 team = body->team;
  if (id == aiKeeper(team)) return AiRole::Keeper;
  // The chaser attacks when its side has the ball and defends when it does
  // not: the same player, two different jobs.
  if (id == aiChaser(team)) return aiHasPossession(team) ? AiRole::Attack : AiRole::Defend;
  return AiRole::Support;
}

// Push away from anyone standing too close. This is what stops two players
// occupying the same spot and deadlocking — the bug where a match froze
// with two opponents stacked on the ball.
Vec3 WorldEditor::aiSeparation(u32 id) const {
  const CharacterBody* self = physics_.characterById(id);
  if (self == nullptr) return Vec3{0.0, 0.0, 0.0};
  Vec3 push{0.0, 0.0, 0.0};
  for (const u32 other : physics_.characterIds()) {
    if (other == id) continue;
    const CharacterBody* body = physics_.characterById(other);
    if (body == nullptr) continue;
    const f64 dx = self->position.x - body->position.x;
    const f64 dz = self->position.z - body->position.z;
    const f64 distance = std::sqrt(dx * dx + dz * dz);
    if (distance >= kAiPersonalSpace) continue;
    if (distance < kMoveEpsilon) {
      // Exactly on top of each other: break the tie with the id so the two
      // of them pick opposite directions instead of both waiting.
      const f64 nudge = id % 2U == 0U ? 1.0 : -1.0;
      push.x += nudge;
      continue;
    }
    // The closer they are, the harder the shove.
    const f64 strength = (kAiPersonalSpace - distance) / kAiPersonalSpace;
    push.x += dx / distance * strength;
    push.z += dz / distance * strength;
  }
  return push;
}

Vec3 WorldEditor::aiTargetFor(u32 id) const {
  const CharacterBody* body = physics_.characterById(id);
  if (body == nullptr || id == kPrimaryCharacter) return Vec3{0.0, 0.0, 0.0};

  // --- Arena fighters (stage 30) ---
  // A shooter has no ball to chase and no net to mind, so the football
  // roles are meaningless here: a "keeper" standing on a goal line in a
  // firefight is just an easy target.
  if (arenaMode()) {
    const f64 boundX = world_.halfWidth() - kPlayerMargin;
    const f64 boundZ = world_.halfLength() - kPlayerMargin;
    // Close to the nearest standing enemy, but stop at a fighting distance
    // rather than walking into their muzzle.
    u32 target = 0U;
    f64 bestDistance = 0.0;
    for (const u32 other : physics_.characterIds()) {
      const CharacterBody* enemy = physics_.characterById(other);
      if (enemy == nullptr || enemy->team == body->team) continue;
      const auto hp = arenaHealth_.find(other);
      if (hp != arenaHealth_.end() && hp->second == 0U) continue;  // ignore the downed
      const f64 dx = enemy->position.x - body->position.x;
      const f64 dz = enemy->position.z - body->position.z;
      const f64 distance = std::sqrt(dx * dx + dz * dz);
      if (target == 0U || distance < bestDistance) {
        target = other;
        bestDistance = distance;
      }
    }
    if (target == 0U) return body->position;  // nobody left standing: hold
    const CharacterBody* enemy = physics_.characterById(target);
    if (enemy == nullptr) return body->position;
    // Hold at engagement range: close enough to shoot, far enough that a
    // firefight is not decided by who bumped into whom.
    const f64 dx = enemy->position.x - body->position.x;
    const f64 dz = enemy->position.z - body->position.z;
    const f64 distance = std::sqrt(dx * dx + dz * dz);
    if (distance < kMoveEpsilon) return body->position;
    const f64 want = distance - kArenaEngageRange;
    // Fan out sideways so a squad does not advance in single file.
    const f64 lane = (static_cast<f64>(id % 3U) - 1.0) * kArenaSpreadOut;
    return Vec3{std::min(boundX, std::max(-boundX, body->position.x + dx / distance * want - dz / distance * lane)),
                body->position.y,
                std::min(boundZ, std::max(-boundZ, body->position.z + dz / distance * want + dx / distance * lane))};
  }

  const SphereBody* ball = physics_.sphere(ballId_);
  if (ball == nullptr) return body->position;
  const u32 team = body->team;
  const f64 forward = attackDirectionZ(team);
  const f64 ownGoalZ = -forward * world_.halfLength();

  // --- Keeper: stay on the line, shuffle across to the ball ---
  if (id == aiKeeper(team)) {
    // A keeper tracks the ball sideways but never wanders far off the
    // line: an empty net is worse than a shot saved.
    const f64 postLimit = kWorldGoalLarge * 0.5;
    const f64 x = std::min(postLimit, std::max(-postLimit, ball->position.x));
    // Comes out a little when the ball is close, back on the line when not.
    const f64 ballDepth = std::abs(ball->position.z - ownGoalZ);
    const f64 comeOut = ballDepth < kAiKeeperRange * 2.0 ? kAiKeeperRange : kAiKeeperRange * 0.35;
    return Vec3{x, body->position.y, ownGoalZ + forward * comeOut};
  }

  const f64 boundX = world_.halfWidth() - kPlayerMargin;
  const f64 limit = world_.halfLength() - kPlayerMargin;

  // --- The player on the ball ---
  if (id == aiChaser(team)) {
    const f64 toBallX = ball->position.x - body->position.x;
    const f64 toBallZ = ball->position.z - body->position.z;
    const f64 range = std::sqrt(toBallX * toBallX + toBallZ * toBallZ);

    // ATTACK: already on the ball, so carry it at their goal rather than
    // standing over it. This is the difference between a game and a scrum:
    // somebody has to actually take the ball somewhere.
    if (range < world_.ball.radius + kAiTackleReach + kAiApproachOffset) {
      // Run AT the goal. This used to stop short: the "trapped ball" rule
      // counted the END wall as trouble, but that is exactly where the net
      // is, so the attacker turned back every time it got close and the
      // score stayed 0-0 forever.
      //
      // Only the SIDE walls trap a ball. A ball in the corner gets taken
      // back infield; a ball near the goal line gets put in the net.
      const f64 wallX = world_.halfWidth() - world_.ball.radius;
      if (std::abs(ball->position.x) > wallX - kAiWallEscape) {
        return Vec3{0.0, body->position.y, ball->position.z};
      }
      const Vec3 mouth = aiGoalMouth(team);
      // Run THROUGH the ball toward the goal, not at the goal directly.
      // Heading straight for the net meant the player could end up on the
      // wrong side of the ball and drag it backwards, which made it
      // judder on the spot instead of advancing.
      const f64 toGoalX = mouth.x - ball->position.x;
      const f64 toGoalZ = mouth.z - ball->position.z;
      const f64 toGoal = std::sqrt(toGoalX * toGoalX + toGoalZ * toGoalZ);
      if (toGoal < kMoveEpsilon) return Vec3{mouth.x, body->position.y, mouth.z};
      // A point a stride beyond the ball, on the line from ball to goal.
      const f64 stepX = ball->position.x + toGoalX / toGoal * kAiGoalAim;
      const f64 stepZ = ball->position.z + toGoalZ / toGoal * kAiGoalAim;
      return Vec3{std::min(boundX, std::max(-boundX, stepX)), body->position.y,
                  std::min(limit, std::max(-limit, stepZ))};
    }

    // DEFEND / close down: approach the ball from our OWN goal side, not
    // its centre. Two chasers aiming at the exact same point met head on
    // and both stopped; arriving from behind it means they end up facing
    // the right way and the ball squirts free instead of jamming.
    return Vec3{ball->position.x, body->position.y,
                std::min(limit, std::max(-limit, ball->position.z - forward * kAiApproachOffset))};
  }

  // --- Everyone else: hold a shape relative to the ball ---
  // Each supporting player gets its OWN slot. The old code used id % 3, so on a
  // five-a-side team ids 7 and 10 were handed the identical spot and piled
  // up on each other. Numbering within the team fixes that.
  u32 slot = 0U;
  u32 mates = 0U;
  for (const u32 other : physics_.characterIds()) {
    if (other == kPrimaryCharacter) continue;
    const CharacterBody* mate = physics_.characterById(other);
    if (mate == nullptr || mate->team != team) continue;
    if (other == aiKeeper(team) || other == aiChaser(team)) continue;
    if (other == id) slot = mates;
    ++mates;
  }
  if (mates == 0U) mates = 1U;
  // Spread the supporting players evenly across the width instead of
  // stacking them: (slot + 1) / (mates + 1) maps to the whole pitch.
  const f64 t = static_cast<f64>(slot + 1U) / static_cast<f64>(mates + 1U);
  f64 lane = -boundX + 2.0 * boundX * t;
  // Keep out of the ball carrier's way. A lane that happens to run through
  // the ball turns the support player into a second attacker and they end
  // up jostling over it, which is the pile-up all over again.
  if (std::abs(lane - ball->position.x) < kAiPersonalSpace) {
    lane += lane < ball->position.x ? -kAiPersonalSpace : kAiPersonalSpace;
  }

  // With the ball, push UP in support so there is an option ahead; without
  // it, drop goal-side and defend. A team that only ever sits behind the
  // ball never attacks.
  const f64 gap = aiHasPossession(team) ? -kAiSupportGap * 0.5 : kAiSupportGap;
  const f64 supportZ = ball->position.z - forward * gap;
  return Vec3{std::min(boundX, std::max(-boundX, lane)), body->position.y,
              std::min(limit, std::max(-limit, supportZ))};
}

void WorldEditor::updateAi(f64 seconds) {
  if (!aiActive() || seconds <= 0.0) return;
  SphereBody* ball = physics_.sphere(ballId_);
  const f64 skill = world_.profile.aiSkill;
  // Skill scales how fast they close you down. Even a perfect one is a
  // shade slower than the human, so a good player can still beat them.
  const f64 speed = world_.player.speed * kAiMaxSpeedFactor * skill;
  const f64 boundX = world_.halfWidth() - kPlayerMargin;
  const f64 boundZ = world_.halfLength() - kPlayerMargin;

  for (auto& cooldown : aiTouchCooldown_) {
    if (cooldown.second > 0.0) cooldown.second = std::max(0.0, cooldown.second - seconds);
  }

  for (const u32 id : physics_.characterIds()) {
    if (id == kPrimaryCharacter) continue;  // the human drives themself
    CharacterBody* body = physics_.characterById(id);
    if (body == nullptr) continue;
    const Vec3 target = aiTargetFor(id);
    const f64 dx = target.x - body->position.x;
    const f64 dz = target.z - body->position.z;
    const f64 distance = std::sqrt(dx * dx + dz * dz);
    Vec3 wanted{0.0, 0.0, 0.0};
    // Do not jitter on the spot once they have arrived.
    if (distance > kMoveEpsilon) {
      // Ease off over the last stride so they settle instead of overshooting.
      const f64 pace = std::min(speed, distance / std::max(seconds, 1e-4));
      wanted = Vec3{dx / distance * pace, 0.0, dz / distance * pace};
    }

    // --- Personal space ---
    // Steer away from anyone crowding this player. Without it two of them
    // walk into each other, each keeps pressing forward, and neither ever
    // moves again.
    const Vec3 apart = aiSeparation(id);
    wanted.x += apart.x * speed * kAiSeparationForce;
    wanted.z += apart.z * speed * kAiSeparationForce;

    // --- Getting unjammed ---
    // Wanting to move but going nowhere means something is in the way.
    // After kAiStuckTime of that, sidestep for a moment to walk around it.
    const bool tryingToMove = distance > kAiStuckDistance;
    const Vec3 previous = aiLastPos_.count(id) > 0U ? aiLastPos_[id] : body->position;
    const f64 travelled =
        std::sqrt(std::pow(body->position.x - previous.x, 2.0) + std::pow(body->position.z - previous.z, 2.0));
    f64& stuckFor = aiStuckFor_[id];
    f64& unstickFor = aiUnstickFor_[id];
    // Moving slower than a crawl while trying to run = something is in the way.
    if (tryingToMove && travelled < kAiStuckDistance * seconds) {
      stuckFor += seconds;
    } else {
      stuckFor = 0.0;
    }
    if (stuckFor > kAiStuckTime) {
      unstickFor = kAiUnstickTime;
      stuckFor = 0.0;
    }
    if (unstickFor > 0.0) {
      unstickFor -= seconds;
      // Strafe across the blocked direction. Odd and even ids go opposite
      // ways, so two players jammed together never pick the same escape.
      const f64 side = id % 2U == 0U ? 1.0 : -1.0;
      const f64 length = std::sqrt(wanted.x * wanted.x + wanted.z * wanted.z);
      if (length > kMoveEpsilon) {
        // Take a copy first: rotating in place would feed the already
        // updated x back into z and bend the sidestep off course.
        const f64 wx = wanted.x;
        const f64 wz = wanted.z;
        wanted.x += -wz / length * speed * side;
        wanted.z += wx / length * speed * side;
      } else {
        wanted.x += speed * side;
      }
    }
    aiLastPos_[id] = body->position;

    // Never ask for more than a run: the pushes above can stack up.
    const f64 wantedSpeed = std::sqrt(wanted.x * wanted.x + wanted.z * wanted.z);
    if (wantedSpeed > speed) {
      wanted.x = wanted.x / wantedSpeed * speed;
      wanted.z = wanted.z / wantedSpeed * speed;
    }
    physics_.moveCharacter(id, seconds, wanted);
    // Keep them on the pitch, like the human.
    body->position.x = std::min(boundX, std::max(-boundX, body->position.x));
    body->position.z = std::min(boundZ, std::max(-boundZ, body->position.z));

    // --- Carrying the ball ---
    // The player on the ball nudges it toward where it is running. Without
    // this an "attacking" player just stands over a stationary ball.
    if (ball != nullptr && aiRole(id) == AiRole::Attack) {
      const f64 ballDx = ball->position.x - body->position.x;
      const f64 ballDz = ball->position.z - body->position.z;
      if (std::sqrt(ballDx * ballDx + ballDz * ballDz) < world_.ball.radius + kAiTackleReach + kAiApproachOffset) {
        // Push it at the NET. Pushing it toward the player's own waypoint
        // sent it sideways or backwards, because that waypoint is a spot
        // beside the ball rather than somewhere to take it.
        // A ball already ON the attacking end line with no net behind it has
        // nowhere to go: aiGoalMouth falls back to a spot on the goal line when
        // the scene has no goal, and a carrier pushing the ball at that spot
        // drives it into the end boards and holds it there — the boards hand it
        // back, the carrier pushes again, and the ball never moves again for the
        // rest of the match. From the line the only place the ball can go is
        // infield, so that is where the carrier plays it. Everywhere else the
        // ball is pushed at the net exactly as before, net or no net: a pitch
        // without nets is still a pitch, and the side still plays up it.
        const f64 forward = attackDirectionZ(body->team);
        const f64 endWall = world_.halfLength() - world_.ball.radius;
        const bool atAttackingLine = forward > 0.0 ? ball->position.z > endWall - kAiWallEscape
                                                   : ball->position.z < -(endWall - kAiWallEscape);
        const Vec3 want = aiGoalMouth(body->team);
        // What to do with the ball is a DECISION, scored, not a chain of ifs:
        // shoot, pass to a team-mate, or carry it yourself (stage 37). The
        // scores are in aiDecision() and the editor can print them.
        const AiDecision choice = aiDecision(id);
        Vec3 aimXZ = want;              // where the ball is being sent
        f64 push = kAiDribblePush;      // how hard
        if (choice.action == AiAction::Shoot) {
          push = kAiShootSpeed;
        } else if (choice.action == AiAction::Pass) {
          const CharacterBody* mate = physics_.characterById(choice.targetId);
          const f64 passDistance =
              mate != nullptr ? std::sqrt((mate->position.x - ball->position.x) * (mate->position.x - ball->position.x) +
                                          (mate->position.z - ball->position.z) * (mate->position.z - ball->position.z))
                              : 0.0;
          // Lead the runner: aim where the team-mate WILL be when the ball
          // arrives, not where they are now — a pass to a walking player's feet
          // is a pass to where the ball was.
          const f64 passSpeed = std::min(kAiPassMaxSpeed, std::max(kAiPassMinSpeed, passDistance * kAiPassSpeedPerMeter));
          const f64 flight = passSpeed > kMoveEpsilon ? passDistance / passSpeed : 0.0;
          const Vec3 predicted = aiPassLeadTarget(choice.targetId, flight);
          aimXZ = Vec3{predicted.x, 0.0, predicted.z};
          push = passSpeed;
        }
        // The end-wall trap still wins over everything: from the line with no
        // net behind it the only way out is infield.
        const bool stuck = atAttackingLine && !goalAtEnd(forward > 0.0);
        const f64 towardX = stuck ? -ball->position.x : aimXZ.x - ball->position.x;
        const f64 towardZ = stuck ? -ball->position.z : aimXZ.z - ball->position.z;
        const f64 towardLength = std::sqrt(towardX * towardX + towardZ * towardZ);
        if (towardLength > kMoveEpsilon) {
          ball->velocity.x = towardX / towardLength * push * skill;
          ball->velocity.z = towardZ / towardLength * push * skill;
          // A pass is a real event: the animation, the audio and a rule can
          // all hang off it, exactly like a shot. One event per TOUCH, though —
          // the push above repeats every frame the player stays on the ball,
          // and six hundred "passes" a minute is not a match.
          if (aiTouchCooldown_[id] <= 0.0) {
            if (choice.action == AiAction::Shoot) {
              events_.push_back(GameEvent::Kick);
              aiTouchCooldown_[id] = kAiTouchCooldown;
            } else if (choice.action == AiAction::Pass && !stuck) {
              events_.push_back(GameEvent::Pass);
              aiTouchCooldown_[id] = kAiTouchCooldown;
            }
          }
        }
      }
    }

    // --- Tackling ---
    // An opponent who reaches the ball knocks it away toward their own
    // attacking end. This is what makes a trick risky: start showing off
    // in front of a defender and they will take it off you.
    //
    // Only a DEFENDER tackles. This used to fire for the attacker too, so
    // the side in possession booted its own ball goal-ward every frame —
    // straight into the end wall, where it stuck and the match froze.
    //
    // BOTH sides tackle. This was once limited to team 2, back when team 2
    // meant "the human's opponents" — but the player's own side is run by
    // the computer too, so that quietly made every match one-way: team 1
    // could hold the ball all game and never be able to win it back or
    // clear it, and finished 0-22.
    if (ball == nullptr) continue;
    if (aiRole(id) != AiRole::Defend) continue;
    const f64 ballDx = ball->position.x - body->position.x;
    const f64 ballDz = ball->position.z - body->position.z;
    if (std::sqrt(ballDx * ballDx + ballDz * ballDz) > world_.ball.radius + kAiTackleReach) continue;
    // A tackle CLEARS the ball away from the tackler, it does not fire it
    // up the pitch. Sending it toward the tackler's attacking end meant
    // every challenge nudged the ball the same way; with one side holding
    // an extra body, that bias piled up until the ball spent 92% of the
    // match in one half and only one team could ever score.
    const f64 awayLength = std::sqrt(ballDx * ballDx + ballDz * ballDz);
    if (awayLength > kMoveEpsilon) {
      // Clear it away, but bend the clearance back INFIELD — on BOTH axes.
      //
      // The x term is the old fix for a defender on the touchline hammering the
      // ball straight out for a throw-in every time, which on a narrow pitch
      // stopped play almost continuously. The z term is the same bug at the
      // other pair of walls, and it was worse than a throw-in: a defender who
      // gets behind a ball near the end line clears it INTO the boards at full
      // power, the boards hold it, and the same touch repeats every frame
      // forever — the ball never moves again and the match freezes on the spot.
      // Nobody can push a ball off the end line either, because that needs a
      // player between the ball and the boards and the pitch ends first.
      const f64 infield = -ball->position.x / std::max(world_.halfWidth(), 1e-6);
      // The z bend is only where it is needed — within a wall's reach of the
      // boards. A bend applied across the whole pitch is not a wall fix, it is
      // a bias: it pulls every clearance toward the centre spot, one half stops
      // being played in, and the match goes one-way again (the test that guards
      // the tackle's fairness caught exactly that).
      const f64 endGap = (world_.halfLength() - kAiWallEscape) - std::abs(ball->position.z);
      const f64 upfield = endGap >= 0.0 ? 0.0
                                        : -std::copysign(std::min(-endGap / kAiWallEscape, 1.0), ball->position.z);
      f64 outX = ballDx / awayLength + infield;
      f64 outZ = ballDz / awayLength + upfield;
      const f64 outLength = std::sqrt(outX * outX + outZ * outZ);
      if (outLength > kMoveEpsilon) {
        outX /= outLength;
        outZ /= outLength;
      }
      ball->velocity.x = outX * kAiTacklePush * skill;
      ball->velocity.z = outZ * kAiTacklePush * skill;
    } else {
      // Dead on top of it: clear it toward the tackler's own attacking end —
      // unless the ball is ALREADY there. A clearance aimed at the end the ball
      // is sitting against is a clearance into the boards, and the boards hand
      // it straight back to the same foot, which repeats every frame: that is
      // the freeze this whole block has been chasing. From up against the end
      // line the only way out is infield, so that is what this does.
      const f64 toward = attackDirectionZ(body->team);
      const f64 endWall = world_.halfLength() - world_.ball.radius;
      const bool onTheLine = std::abs(ball->position.z) > endWall - kAiWallEscape;
      ball->velocity.z = (onTheLine ? -toward : toward) * kAiTacklePush * skill;
    }
    // A challenge is a touch too: the clearance repeats while the defender
    // stays in reach, but the tackle is one event, not one per frame.
    if (aiTouchCooldown_[id] <= 0.0) {
      events_.push_back(GameEvent::Tackle);
      aiTouchCooldown_[id] = kAiTouchCooldown;
    }
  }
}

}  // namespace kimia
