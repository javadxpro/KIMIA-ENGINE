// Match rules: what happens between the goals.
//
// Split out of World.cpp with no behaviour change. What it owns:
//
//   * the stoppage machine (throw-in, corner, goal kick, penalty) and the
//     restart the whistle awards,
//   * offside,
//   * stamina and the pace it leaves the player with,
//   * the pace itself (setPlayerPace/playerPace) and the motor that can
//     override it, so "how fast the player moves" has one home,
//   * the per-frame rules step (updateRules) and the line the HUD shows.
#include "WorldInternal.h"

#include <kimia/World.h>

#include <algorithm>
#include <cmath>

namespace kimia {

using namespace worldinternal;  // the World module's own toolbox (see the header)


const char* WorldEditor::stoppageName(Stoppage stoppage) {
  switch (stoppage) {
    case Stoppage::ThrowIn: return "THROW IN";
    case Stoppage::Offside: return "OFFSIDE";
    case Stoppage::Foul: return "FOUL";
    case Stoppage::None: break;
  }
  return "";
}

std::string WorldEditor::rulesHudText() const {
  if (stoppage_ == Stoppage::None) return std::string();
  // Whose ball it is matters more than the decision itself.
  const std::string side = restartTeam_ == 1U ? "MA BALL" : "ANHA BALL";
  return std::string(stoppageName(stoppage_)) + "  " + side;
}

const CharacterMotorComponent* WorldEditor::playerMotor() const {
  // The driven character is the entity named "Player" — the same rule the rest
  // of the editor uses to find it (playerRest, playerEntity).
  return characterMotor("Player");
}

void WorldEditor::setPlayerPace(f64 speed) {
  world_.player.speed = speed;
  // One number for the menu and the feet: if the driven entity has a motor,
  // the menu writes the motor's top speed, because that is what moves it.
  EntityData* player = world_.scene.get(playerEntity());
  if (player != nullptr && player->motor.has_value()) {
    player->motor->maxSpeed = speed;
  }
}

f64 WorldEditor::playerPace() const {
  const CharacterMotorComponent* motor = playerMotor();
  return motor != nullptr ? motor->maxSpeed : world_.player.speed;
}

f64 WorldEditor::currentPlayerSpeed() const {
  // A motor on the driven entity owns the pace; without one this is the
  // world's own number, exactly as before.
  const f64 topSpeed = playerPace();
  // No stamina in the profile means the endless runner every other game
  // has always had: full pace, forever.
  if (world_.profile.stamina <= 0.0) return topSpeed;
  // A spent player is slower, never stopped: you tire, you do not seize up.
  const f64 pace = kRulesTiredPace + (1.0 - kRulesTiredPace) * stamina_;
  return topSpeed * pace;
}

void WorldEditor::updateStamina(f64 seconds, bool running) {
  // A profile without stamina keeps the endless runner. The rate below
  // would already be zero, so this is not what makes that true — it just
  // pins the value at exactly 1.0 and skips the arithmetic.
  if (world_.profile.stamina <= 0.0) {
    stamina_ = 1.0;
    return;
  }
  // Running drains, standing recovers — and recovery is slower than the
  // drain, so you cannot sprint the whole match.
  const f64 rate = running ? -kRulesStaminaDrain * world_.profile.stamina
                           : kRulesStaminaRecover * world_.profile.stamina;
  stamina_ += rate * seconds;
  if (stamina_ < 0.0) stamina_ = 0.0;
  if (stamina_ > 1.0) stamina_ = 1.0;
}

void WorldEditor::awardRestart(Stoppage reason, u32 team, const Vec3& spot) {
  stoppage_ = reason;
  restartTeam_ = team;
  restartSpot_ = spot;
  restartTimer_ = kRulesRestartPause;
  // Dead ball: park it on the restart spot and stop everything.
  SphereBody* ball = physics_.sphere(ballId_);
  if (ball != nullptr) {
    ball->position = Vec3{spot.x, world_.ball.radius, spot.z};
    ball->velocity = Vec3{0.0, 0.0, 0.0};
    ball->spin = Vec3{0.0, 0.0, 0.0};
  }
  // A trick in progress is over: the whistle has gone.
  trick_ = Trick::None;
  trickTimer_ = 0.0;
  trickLength_ = 0.0;
  events_.push_back(GameEvent::Whistle);
}

// Is a team-mate beyond the last defender, and therefore offside? Level is
// ONSIDE, which is the rule people always get wrong.
bool WorldEditor::offsideFor(u32 id) const {
  if (!world_.profile.rules) return false;
  const CharacterBody* mate = physics_.characterById(id);
  if (mate == nullptr || mate->team != 1U) return false;
  // Team 1 attacks -Z, so "further forward" means a smaller z.
  // Find the last defender: the opponent nearest their own goal line.
  f64 lastDefenderZ = -world_.halfLength();
  bool found = false;
  for (const u32 other : physics_.characterIds()) {
    const CharacterBody* body = physics_.characterById(other);
    if (body == nullptr || body->team != 2U) continue;
    // The offside line is the opposition's HIGHEST defender: team 1
    // attacks -Z, so that is the opponent with the largest z. Anyone in
    // front of that line has nobody left to play them onside.
    if (!found || body->position.z > lastDefenderZ) {
      lastDefenderZ = body->position.z;
      found = true;
    }
  }
  if (!found) return false;
  // Offside only in the opposition half.
  if (mate->position.z > 0.0) return false;
  return mate->position.z < lastDefenderZ - kRulesOffsideMargin;
}

// Watches for the ball leaving the pitch and for reckless challenges.
// `previousBall` is where the ball was before this frame's physics, so a
// ball that shot out and got clamped back is still caught.
void WorldEditor::updateRules(f64 seconds, const Vec3& previousBall) {
  if (!world_.profile.rules) return;

  // --- Serving a stoppage ---
  if (stoppage_ != Stoppage::None) {
    restartTimer_ -= seconds;
    // Hold the ball still on the spot while the whistle has gone.
    SphereBody* held = physics_.sphere(ballId_);
    if (held != nullptr) {
      held->position = Vec3{restartSpot_.x, world_.ball.radius, restartSpot_.z};
      held->velocity = Vec3{0.0, 0.0, 0.0};
    }
    if (restartTimer_ <= 0.0) {
      // Ball is live again.
      stoppage_ = Stoppage::None;
      restartTimer_ = 0.0;
      restartTeam_ = 0U;
    }
    return;
  }

  const SphereBody* ball = physics_.sphere(ballId_);
  if (ball == nullptr) return;

  // --- Out of play: a touchline throw-in ---
  // The ball is clamped inside the pitch by the play loop, so check where
  // it WANTED to go rather than where it ended up.
  const f64 touchline = world_.halfWidth() - world_.ball.radius;
  if (std::abs(previousBall.x) >= touchline - 1e-6 && std::abs(ball->position.x) >= touchline - 1e-6) {
    // Whoever did NOT put it out gets the throw. The player is team 1, so
    // if the player was the last to touch it, it is theirs.
    const f64 side = ball->position.x > 0.0 ? 1.0 : -1.0;
    const u32 team = 2U;  // conceded by the side in possession (the player)
    awardRestart(Stoppage::ThrowIn, team,
                 Vec3{side * (touchline - kRulesRestartInset), 0.0, ball->position.z});
    return;
  }

  // --- Offside ---
  // Flagged when the ball is played to a team-mate who was beyond the last
  // defender. Checked against the pass target, which is who the ball is
  // actually going to.
  // Only when the HUMAN actually plays the ball. This used to run every
  // frame against whoever happened to be furthest forward, so once the
  // computer players started making runs the flag went up 44 times a
  // minute and the match was stopped 95% of the time.
  if (humanPassedBall_) {
    humanPassedBall_ = false;
    const u32 target = passTarget();
    if (target != 0U && offsideFor(target)) {
      const CharacterBody* mate = physics_.characterById(target);
      if (mate != nullptr) {
        awardRestart(Stoppage::Offside, 2U, Vec3{mate->position.x, 0.0, mate->position.z});
        return;
      }
    }
  }

  // --- Fouls ---
  // Charging into an opponent at speed is a foul. A tackle for the ball is
  // fair; running through somebody is not.
  const Vec3 playerVelocity = physics_.character() != nullptr ? physics_.character()->velocity : Vec3{0.0, 0.0, 0.0};
  const f64 chargeSpeed = std::sqrt(playerVelocity.x * playerVelocity.x + playerVelocity.z * playerVelocity.z);
  if (chargeSpeed <= kRulesFoulSpeed) return;
  for (const u32 id : physics_.characterIds()) {
    if (id == kPrimaryCharacter) continue;
    const CharacterBody* other = physics_.characterById(id);
    if (other == nullptr || other->team == 1U) continue;
    const f64 dx = other->position.x - playerPos_.x;
    const f64 dz = other->position.z - playerPos_.z;
    const f64 distance = std::sqrt(dx * dx + dz * dz);
    if (distance > kWorldPlayerRadius * 2.0) continue;
    // Only a charge INTO them counts: running away from somebody is not a
    // foul however fast you do it.
    if (dx * playerVelocity.x + dz * playerVelocity.z <= 0.0) continue;
    awardRestart(Stoppage::Foul, 2U, Vec3{other->position.x, 0.0, other->position.z});
    return;
  }
}

}  // namespace kimia
