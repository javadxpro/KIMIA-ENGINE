#include <kimia/StreetSoccerPhysics.h>

namespace kimia::street {

bool setupStreetPhysics(StreetPhysicsBridge& bridge,
                        PhysicsWorld& world,
                        const Pitch& pitch) {
  bridge.world = &world;
  // Ground at y=0.
  bridge.groundBodyId = world.addPlane(0.0);
  if (bridge.groundBodyId == 0) return false;

  // Side walls: long thin boxes along the length, on +Z and -Z edges.
  const f32 halfZ = pitch.width * 0.5f + 0.5f;
  const f32 halfL = pitch.length * 0.5f;
  world.addBox(Vec3{0.0, 0.5,  halfZ},
               Vec3{halfL + 1.0, 0.5, 0.5});
  world.addBox(Vec3{0.0, 0.5, -halfZ},
               Vec3{halfL + 1.0, 0.5, 0.5});

  // End walls behind the goals.
  const f32 halfW = pitch.width * 0.5f + 0.5f;
  world.addBox(Vec3{ halfL + 1.0, 0.5, 0.0},
               Vec3{0.5, 0.5, halfW});
  world.addBox(Vec3{-halfL - 1.0, 0.5, 0.0},
               Vec3{0.5, 0.5, halfW});
  return true;
}

void spawnBallBody(StreetPhysicsBridge& bridge,
                   PhysicsWorld& world,
                   const Ball& ball) {
  SphereBody b;
  b.position  = Vec3{ball.x, ball.radius, ball.y};
  b.velocity  = Vec3{ball.vx, 0.0, ball.vy};
  b.radius    = ball.radius;
  b.mass      = ball.mass;
  b.restitution = 0.6f;
  b.friction    = 0.45f;
  bridge.ballBodyId = world.addSphere(b);
}

void spawnPlayerBody(StreetPhysicsBridge& bridge,
                     PhysicsWorld& world,
                     const Player& player,
                     f32 radius) {
  SphereBody b;
  b.position = Vec3{player.positionX, radius, player.positionY};
  b.velocity = Vec3{player.velocityX, 0.0, player.velocityY};
  b.radius   = radius;
  b.mass     = 70.0f;   // ~70 kg average street player
  b.restitution = 0.20f;
  b.friction    = 0.55f;
  const u32 id = world.addSphere(b);
  bridge.playerBodies[player.shirtNumber] = id;
}

void syncFromPhysics(StreetPhysicsBridge& bridge,
                     MatchState& state,
                     f32 /*dt*/) {
  if (!bridge.world) return;

  // Ball.
  if (bridge.ballBodyId != 0) {
    if (const SphereBody* b = bridge.world->sphere(bridge.ballBodyId)) {
      state.ball.x  = static_cast<f32>(b->position.x);
      state.ball.y  = static_cast<f32>(b->position.z);
      state.ball.vx = static_cast<f32>(b->velocity.x);
      state.ball.vy = static_cast<f32>(b->velocity.z);
    }
  }
  // Players.
  auto syncTeam = [&](std::vector<Player>& team) {
    for (auto& p : team) {
      const auto it = bridge.playerBodies.find(p.shirtNumber);
      if (it == bridge.playerBodies.end()) continue;
      if (const SphereBody* b = bridge.world->sphere(it->second)) {
        p.positionX = static_cast<f32>(b->position.x);
        p.positionY = static_cast<f32>(b->position.z);
        p.velocityX = static_cast<f32>(b->velocity.x);
        p.velocityY = static_cast<f32>(b->velocity.z);
      }
    }
  };
  syncTeam(state.homeTeam);
  syncTeam(state.awayTeam);
}

void kickBall(StreetPhysicsBridge& bridge,
              f32 impulseX,
              f32 impulseY) {
  if (!bridge.world) return;
  if (bridge.ballBodyId == 0) return;
  if (SphereBody* b = bridge.world->sphere(bridge.ballBodyId)) {
    b->velocity.x += impulseX;
    b->velocity.z += impulseY;
  }
}

void pushPlayer(StreetPhysicsBridge& bridge,
                u32 shirtNumber,
                f32 impulseX,
                f32 impulseY) {
  if (!bridge.world) return;
  const auto it = bridge.playerBodies.find(shirtNumber);
  if (it == bridge.playerBodies.end()) return;
  if (SphereBody* b = bridge.world->sphere(it->second)) {
    b->velocity.x += impulseX;
    b->velocity.z += impulseY;
  }
}

void teleportBall(StreetPhysicsBridge& bridge, f32 x, f32 y) {
  if (!bridge.world) return;
  if (bridge.ballBodyId == 0) return;
  if (SphereBody* b = bridge.world->sphere(bridge.ballBodyId)) {
    b->position.x = x;
    b->position.z = y;
    b->velocity.x = 0;
    b->velocity.z = 0;
    b->velocity.y = 0;
  }
}

}  // namespace kimia::street
