// Arena mode: the last-one-standing game the profile can switch on.
//
// Split out of World.cpp with no behaviour change. Health and ammo are STATE
// (the queries at the top of the file), the rest is the round: who shoots,
// when a reload finishes, who is left standing, and the line the HUD shows.
//
// The AI that fights in this mode is not here — it is the same computer
// players every other mode uses (Engine/World/src/Ai.cpp); a profile only
// changes what they aim at.
#include "WorldInternal.h"

#include <kimia/World.h>

#include <algorithm>
#include <cmath>

namespace kimia {

using namespace worldinternal;  // the World module's own toolbox (see the header)


u32 WorldEditor::health(u32 id) const {
  if (!arenaMode()) return 0U;
  const auto found = arenaHealth_.find(id);
  return found == arenaHealth_.end() ? 0U : found->second;
}

u32 WorldEditor::ammo(u32 id) const {
  if (!arenaMode()) return 0U;
  const auto found = arenaAmmo_.find(id);
  return found == arenaAmmo_.end() ? 0U : found->second;
}

bool WorldEditor::reloading(u32 id) const {
  if (!arenaMode()) return false;
  const auto found = arenaReload_.find(id);
  return found != arenaReload_.end() && found->second > 0.0;
}

u32 WorldEditor::arenaScore(u32 team) const {
  if (team == 1U) return arenaKills1_;
  if (team == 2U) return arenaKills2_;
  return 0U;
}

void WorldEditor::arenaReset() {
  arenaHealth_.clear();
  arenaAmmo_.clear();
  arenaReload_.clear();
  arenaCooldown_.clear();
  arenaRespawn_.clear();
  if (!arenaMode()) return;
  for (const u32 id : physics_.characterIds()) {
    arenaHealth_[id] = world_.profile.health;
    arenaAmmo_[id] = world_.profile.magazine;
  }
}

std::string WorldEditor::arenaHudText() const {
  if (!arenaMode()) return std::string();
  const u32 hp = health(kPrimaryCharacter);
  if (hp == 0U) return "DOWN";
  if (reloading(kPrimaryCharacter)) return "HP " + std::to_string(hp) + "  RELOADING";
  return "HP " + std::to_string(hp) + "  AMMO " + std::to_string(ammo(kPrimaryCharacter)) + "/" +
         std::to_string(world_.profile.magazine);
}

// One fighter pulls the trigger along `aim`. Everything a shot needs to be
// fair is checked here, so the human and the computer use the identical
// path — no special cases that quietly favour one of them.
bool WorldEditor::arenaShoot(u32 id, const Vec3& aim) {
  if (!arenaMode() || !playing() || roundOver()) return false;
  if (health(id) == 0U) return false;      // downed fighters do not shoot
  if (reloading(id)) return false;         // nor mid-reload
  if (arenaAmmo_[id] == 0U) return false;  // nor with an empty magazine
  const auto cooldown = arenaCooldown_.find(id);
  if (cooldown != arenaCooldown_.end() && cooldown->second > 0.0) return false;  // rate of fire

  const CharacterBody* body = physics_.characterById(id);
  if (body == nullptr) return false;
  const f64 length = std::sqrt(aim.x * aim.x + aim.y * aim.y + aim.z * aim.z);
  if (length < kMoveEpsilon) return false;
  const Vec3 direction{aim.x / length, aim.y / length, aim.z / length};
  const Vec3 muzzle{body->position.x, body->position.y + kArenaMuzzleHeight, body->position.z};

  --arenaAmmo_[id];
  arenaCooldown_[id] = 1.0 / world_.profile.fireRate;

  const PhysicsWorld::RayHit shot = physics_.raycast(muzzle, direction, world_.profile.range, id);
  lastShotFrom_ = muzzle;
  lastShotTo_ = shot.hit ? shot.point : muzzle + direction * world_.profile.range;
  lastShotHit_ = false;

  if (shot.hit && shot.character != 0U) {
    const CharacterBody* target = physics_.characterById(shot.character);
    // Friendly fire does no damage: teams would shred each other in the
    // scramble and the match would be decided by accidents.
    if (target != nullptr && target->team != body->team) {
      u32& hp = arenaHealth_[shot.character];
      hp = hp > world_.profile.damage ? hp - world_.profile.damage : 0U;
      lastShotHit_ = true;
      if (hp == 0U) {
        arenaRespawn_[shot.character] = kArenaRespawnTime;
        if (body->team == 1U) {
          ++arenaKills1_;
        } else {
          ++arenaKills2_;
        }
        events_.push_back(GameEvent::Goal);  // a downed opponent is the score here
      } else {
        events_.push_back(GameEvent::Tackle);  // the hit marker
      }
    }
  }
  events_.push_back(GameEvent::Shot);
  return true;
}

bool WorldEditor::fire() { return arenaShoot(kPrimaryCharacter, aimDirection()); }

bool WorldEditor::reload() {
  if (!arenaMode() || health(kPrimaryCharacter) == 0U) return false;
  if (reloading(kPrimaryCharacter)) return false;
  if (arenaAmmo_[kPrimaryCharacter] >= world_.profile.magazine) return false;  // already full
  arenaReload_[kPrimaryCharacter] = world_.profile.reloadTime;
  return true;
}

void WorldEditor::updateArena(f64 seconds) {
  if (!arenaMode() || seconds <= 0.0) return;
  // Late joiners (a character spawned after the reset) start whole.
  for (const u32 id : physics_.characterIds()) {
    if (arenaHealth_.find(id) == arenaHealth_.end()) {
      arenaHealth_[id] = world_.profile.health;
      arenaAmmo_[id] = world_.profile.magazine;
    }
  }

  for (const u32 id : physics_.characterIds()) {
    // Rate of fire.
    f64& cooldown = arenaCooldown_[id];
    if (cooldown > 0.0) cooldown = cooldown > seconds ? cooldown - seconds : 0.0;

    // Reloads.
    f64& reloadLeft = arenaReload_[id];
    if (reloadLeft > 0.0) {
      reloadLeft -= seconds;
      if (reloadLeft <= 0.0) {
        reloadLeft = 0.0;
        arenaAmmo_[id] = world_.profile.magazine;
      }
    }

    // Respawns: a downed fighter comes back at their own end, whole again.
    if (arenaHealth_[id] == 0U) {
      f64& respawn = arenaRespawn_[id];
      respawn -= seconds;
      if (respawn <= 0.0) {
        respawn = 0.0;
        arenaHealth_[id] = world_.profile.health;
        arenaAmmo_[id] = world_.profile.magazine;
        CharacterBody* body = physics_.characterById(id);
        if (body != nullptr) {
          const f64 ownEnd = body->team == 1U ? world_.halfLength() - kPlayerMargin
                                              : -(world_.halfLength() - kPlayerMargin);
          body->position.z = ownEnd;
          body->velocity = Vec3{0.0, 0.0, 0.0};
          if (id == kPrimaryCharacter) playerPos_ = body->position;
        }
      }
      continue;  // no shooting while down
    }

    // An empty magazine reloads itself: nobody stands in a firefight
    // holding an empty rifle waiting to be told.
    if (arenaAmmo_[id] == 0U && reloadLeft <= 0.0) {
      arenaReload_[id] = world_.profile.reloadTime;
      continue;
    }

    if (id == kPrimaryCharacter) {
      // The human's held trigger fires at the weapon's rate.
      if (fireHeld_) arenaShoot(id, aimDirection());
      continue;
    }

    // --- Computer fighters ---
    if (!aiActive()) continue;
    const CharacterBody* body = physics_.characterById(id);
    if (body == nullptr) continue;
    // Shoot at the nearest standing opponent that is roughly in front.
    u32 target = 0U;
    f64 bestDistance = 0.0;
    for (const u32 other : physics_.characterIds()) {
      const CharacterBody* enemy = physics_.characterById(other);
      if (enemy == nullptr || enemy->team == body->team) continue;
      if (arenaHealth_[other] == 0U) continue;  // do not shoot the downed
      const f64 dx = enemy->position.x - body->position.x;
      const f64 dz = enemy->position.z - body->position.z;
      const f64 distance = std::sqrt(dx * dx + dz * dz);
      if (distance > world_.profile.range) continue;
      if (target == 0U || distance < bestDistance) {
        target = other;
        bestDistance = distance;
      }
    }
    if (target == 0U) continue;
    const CharacterBody* enemy = physics_.characterById(target);
    if (enemy == nullptr) continue;
    Vec3 aim{enemy->position.x - body->position.x, 0.0, enemy->position.z - body->position.z};
    const f64 aimLength = std::sqrt(aim.x * aim.x + aim.z * aim.z);
    if (aimLength < kMoveEpsilon) continue;
    aim.x /= aimLength;
    aim.z /= aimLength;
    // Skill spoils the aim: a perfect AI would be a dead shot at any range
    // and no human could ever win. The wobble is deterministic — derived
    // from the ids — so a replay of the same match plays out the same way.
    const f64 spread = kArenaAiSpread * (1.0 - world_.profile.aiSkill);
    const f64 wobble = spread * (static_cast<f64>((id * 7U + target * 13U) % 11U) / 5.0 - 1.0);
    const f64 sin = std::sin(wobble);
    const f64 cos = std::cos(wobble);
    const Vec3 spread_aim{aim.x * cos - aim.z * sin, 0.0, aim.x * sin + aim.z * cos};
    arenaShoot(id, spread_aim);
  }
}

}  // namespace kimia
