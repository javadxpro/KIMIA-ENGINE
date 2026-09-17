#include <kimia_test.h>
#include <kimia/StreetSoccerPhysics.h>
#include <kimia/Physics.h>
#include <cstdio>

using namespace kimia::street;
using namespace kimia;

static int g_pass = 0;
static int g_fail = 0;
#define EXPECT(cond) do { \
  if (cond) { ++g_pass; } \
  else      { ++g_fail; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } \
} while (0)
#define EXPECT_EQ(a, b) do { \
  auto va = (a); auto vb = (b); \
  if (va == vb) { ++g_pass; } \
  else { ++g_fail; std::printf("FAIL %s:%d %s == %s\n", __FILE__, __LINE__, #a, #b); } \
} while (0)

KIMIA_TEST(SSP_SetupCreatesGround) {
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p;
  EXPECT(setupStreetPhysics(b, world, p));
  EXPECT(b.groundBodyId != 0);
}

KIMIA_TEST(SSP_SetupWithCustomPitch) {
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p;
  p.length = 40.0f;
  p.width = 20.0f;
  EXPECT(setupStreetPhysics(b, world, p));
  EXPECT(b.groundBodyId != 0);
}

KIMIA_TEST(SSP_SpawnBallBody) {
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p; setupStreetPhysics(b, world, p);
  Ball ball;
  ball.x = 1.0f; ball.y = 2.0f;
  spawnBallBody(b, world, ball);
  EXPECT(b.ballBodyId != 0);
}

KIMIA_TEST(SSP_SpawnPlayerBody) {
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p; setupStreetPhysics(b, world, p);
  Player pl;
  pl.shirtNumber = 7;
  pl.positionX = 5.0f;
  pl.positionY = -2.0f;
  spawnPlayerBody(b, world, pl);
  EXPECT(b.playerBodies.count(7) > 0);
}

KIMIA_TEST(SSP_SpawnManyPlayers) {
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p; setupStreetPhysics(b, world, p);
  for (u8 n : {1, 7, 10, 11, 9, 23}) {
    Player pl; pl.shirtNumber = n;
    pl.positionX = 3.0f; pl.positionY = 0.0f;
    spawnPlayerBody(b, world, pl);
  }
  EXPECT_EQ(b.playerBodies.size(), 6u);
}

KIMIA_TEST(SSP_KickBallAddsVelocity) {
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p; setupStreetPhysics(b, world, p);
  Ball ball; spawnBallBody(b, world, ball);
  kickBall(b, 10.0f, 0.0f);
  if (const SphereBody* sp = world.sphere(b.ballBodyId)) {
    EXPECT(sp->velocity.x > 9.0f);
  } else {
    EXPECT(false);
  }
}

KIMIA_TEST(SSP_KickBallZeroImpulseNoChange) {
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p; setupStreetPhysics(b, world, p);
  Ball ball; spawnBallBody(b, world, ball);
  kickBall(b, 0.0f, 0.0f);
  if (const SphereBody* sp = world.sphere(b.ballBodyId)) {
    EXPECT_EQ(sp->velocity.x, 0.0);
    EXPECT_EQ(sp->velocity.z, 0.0);
  }
}

KIMIA_TEST(SSP_PushPlayerAddsVelocity) {
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p; setupStreetPhysics(b, world, p);
  Player pl; pl.shirtNumber = 10;
  spawnPlayerBody(b, world, pl);
  pushPlayer(b, 10, 5.0f, 2.0f);
  if (const SphereBody* sp = world.sphere(b.playerBodies[10])) {
    EXPECT(sp->velocity.x > 4.0f);
    EXPECT(sp->velocity.z > 1.0f);
  }
}

KIMIA_TEST(SSP_PushUnknownPlayerSafe) {
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p; setupStreetPhysics(b, world, p);
  // Should not crash when player id is not registered.
  pushPlayer(b, 99, 1.0f, 1.0f);
  EXPECT_EQ(b.playerBodies.count(99), 0u);
}

KIMIA_TEST(SSP_TeleportBall) {
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p; setupStreetPhysics(b, world, p);
  Ball ball; spawnBallBody(b, world, ball);
  teleportBall(b, 5.0f, -3.0f);
  if (const SphereBody* sp = world.sphere(b.ballBodyId)) {
    EXPECT_EQ(sp->position.x, 5.0);
    EXPECT_EQ(sp->position.z, -3.0);
    EXPECT_EQ(sp->velocity.x, 0.0);
  }
}

KIMIA_TEST(SSP_SyncBallFromPhysics) {
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p; setupStreetPhysics(b, world, p);
  MatchState m;
  buildDefaultTeams(m);
  spawnBallBody(b, world, m.ball);
  // Drive ball with a kick then step.
  kickBall(b, 5.0f, 0.0f);
  for (int i = 0; i < 60; ++i) world.step();
  syncFromPhysics(b, m, 1.0f);
  // After 1 second of physics, ball should have moved noticeably.
  EXPECT(std::abs(m.ball.x) > 0.1 || std::abs(m.ball.y) > 0.1);
}

KIMIA_TEST(SSP_SyncPlayersFromPhysics) {
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p; setupStreetPhysics(b, world, p);
  MatchState m;
  buildDefaultTeams(m);
  for (auto& pl : m.homeTeam) spawnPlayerBody(b, world, pl);
  for (auto& pl : m.awayTeam) spawnPlayerBody(b, world, pl);
  pushPlayer(b, 7, 4.0f, 0.0f);
  for (int i = 0; i < 60; ++i) world.step();
  syncFromPhysics(b, m, 1.0f);
  // Player 7 (Garrincha) should have moved.
  for (auto& pl : m.homeTeam) {
    if (pl.shirtNumber == 7) {
      EXPECT(std::abs(pl.positionX) > 0.1 || std::abs(pl.positionY) > 0.1);
    }
  }
}

KIMIA_TEST(SSP_BallRespectsBoundaries) {
  // After many seconds of physics, the ball should not escape the pitch box.
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p; setupStreetPhysics(b, world, p);
  MatchState m;
  buildDefaultTeams(m);
  spawnBallBody(b, world, m.ball);
  kickBall(b, 30.0f, 30.0f);
  for (int i = 0; i < 600; ++i) world.step();
  if (const SphereBody* sp = world.sphere(b.ballBodyId)) {
    EXPECT(std::abs(sp->position.x) < p.length);
    EXPECT(std::abs(sp->position.z) < p.width);
  }
}

KIMIA_TEST(SSP_KickoffScenario) {
  // After teleportBall(0,0), a kick should send the ball into play.
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p; setupStreetPhysics(b, world, p);
  MatchState m;
  buildDefaultTeams(m);
  spawnBallBody(b, world, m.ball);
  teleportBall(b, 0.0f, 0.0f);
  kickBall(b, 8.0f, 0.0f);
  for (int i = 0; i < 30; ++i) world.step();
  syncFromPhysics(b, m, 0.5f);
  EXPECT(m.ball.x > 0.5f);
}

KIMIA_TEST(SSP_BallAtRestSettles) {
  PhysicsWorld world;
  StreetPhysicsBridge b;
  Pitch p; setupStreetPhysics(b, world, p);
  MatchState m;
  buildDefaultTeams(m);
  spawnBallBody(b, world, m.ball);
  // Run for several seconds without external force.
  for (int i = 0; i < 600; ++i) world.step();
  if (const SphereBody* sp = world.sphere(b.ballBodyId)) {
    // Ball should have stopped or be barely moving.
    const double speed2 =
        sp->velocity.x * sp->velocity.x +
        sp->velocity.z * sp->velocity.z;
    EXPECT(speed2 < 4.0);
  }
}
