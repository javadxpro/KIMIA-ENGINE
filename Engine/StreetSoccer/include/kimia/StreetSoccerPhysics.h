#pragma once
// =============================================================================
//  Street Soccer Physics Bridge
//
//  Connects the Street Soccer data layer (Player, Ball, Pitch) to the engine
//  PhysicsWorld. The bridge is one-way:
//
//      MatchState <-- sync -- PhysicsWorld
//
//  Physics drives the simulation; the MatchState is updated each frame from
//  the rigid bodies. We do not modify PhysicsWorld — only read from it.
//
//  This is intentionally lightweight: ball is a dynamic sphere, players are
//  dynamic spheres too (street rules let players push each other around).
// =============================================================================

#include <kimia/StreetSoccer.h>
#include <kimia/Physics.h>
#include <unordered_map>

namespace kimia::street {

struct StreetPhysicsBridge {
  // One PhysicsWorld per match.
  PhysicsWorld* world = nullptr;

  // Body id lookup.
  u32 ballBodyId        = 0;
  u32 groundBodyId      = 0;
  u32 homeWallBodyId    = 0;
  u32 awayWallBodyId    = 0;
  std::unordered_map<u32, u32> playerBodies;  // shirtNumber -> bodyId
};

// Build the static geometry (ground + side walls + end walls).
// Returns true if setup succeeded.
bool setupStreetPhysics(StreetPhysicsBridge& bridge,
                        PhysicsWorld& world,
                        const Pitch& pitch);

// Spawn the ball as a dynamic sphere.
void spawnBallBody(StreetPhysicsBridge& bridge,
                   PhysicsWorld& world,
                   const Ball& ball);

// Spawn one player as a dynamic sphere.
// shirtNumber is the key; reuse the same id to update.
void spawnPlayerBody(StreetPhysicsBridge& bridge,
                     PhysicsWorld& world,
                     const Player& player,
                     f32 radius = 0.4f);

// Step the physics world and write the result back into the MatchState.
void syncFromPhysics(StreetPhysicsBridge& bridge,
                     MatchState& state,
                     f32 dt);

// Apply a kick to the ball. Adds to the ball's linear velocity.
void kickBall(StreetPhysicsBridge& bridge,
              f32 impulseX,
              f32 impulseY);

// Apply a push to a player body.
void pushPlayer(StreetPhysicsBridge& bridge,
                u32 shirtNumber,
                f32 impulseX,
                f32 impulseY);

// Set ball position (e.g. for kickoff / out-of-bounds reset).
void teleportBall(StreetPhysicsBridge& bridge, f32 x, f32 y);

}  // namespace kimia::street
