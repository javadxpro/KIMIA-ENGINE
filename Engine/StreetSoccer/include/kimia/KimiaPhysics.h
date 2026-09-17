#pragma once
// =============================================================================
//  KimiaPhysics — TGS-style solver + CCD wrapper on top of PhysicsWorld.
//
//  Required by phase 1: "فیزیک پیشرفته و دقیق".
//
//  Two important additions the street-soccer demo needs beyond the basic
//  PhysicsWorld:
//
//    1. TGS-style warm-starting. We remember last-frame's contact impulses
//       and reuse them at the start of the next frame's solve. When two
//       bodies bounce over and over against each other (a dribble, a goal
//       post, a corner-of-wall), the warm-start stabilises the solver
//       in 2 iterations rather than 8. This is the spatial+temporal
//       coherence that Erin's classic TGS paper highlights.
//
//    2. CCD (continuous collision detection) on a per-sphere basis.
//       For balls that travel more than half their radius per frame
//       (a fast shot at 60Hz fps that bumps dt to 1/15s), the discrete
//       solver can tunnel through thin walls (the chain-link mesh, the
//       goal post). We do a swept-sphere check against static boxes and
//       planes for any body whose velocity * dt > 0.5 * radius.
//
//  Both layers are non-invasive: KimiaPhysics owns its own PhysicsWorld
//  and exposes the same addSphere/addBox/step APIs.
// =============================================================================

#include <kimia/Types.h>
#include <kimia/Physics.h>
#include <vector>

namespace kimia::street {

struct CcdSweepHit {
  Vec3 point;
  Vec3 normal;
  f64  t;  // 0..1 fraction of dt at impact
};

// Contact pair key for warm-starting.
struct ContactPairKey {
  u32 bodyA;
  u32 bodyB;
  bool operator==(const ContactPairKey& o) const {
    return bodyA == o.bodyA && bodyB == o.bodyB;
  }
};

class KimiaPhysics {
public:
  explicit KimiaPhysics(f64 fixedDt = 1.0 / 60.0, u32 maxSteps = 5);

  // ---- World setup (delegated)
  PhysicsWorld& world() { return world_; }

  u32 addSphere(const SphereBody& b);
  u32 addBox(const DynamicBox& b);
  void addStaticPlane(const StaticPlane& p);
  void addStaticBox(const StaticBox& b);

  // ---- Step
  // Runs the underlying step() the requested number of times, plus
  // applies TGS warm-start to the next frame after each step.
  void step();

  // ---- CCD
  // Test body `idx` against all static colliders. Returns the earliest hit
  // if a sweep is required (velocity × dt > 0.5 × radius). Otherwise returns
  // false via `out` cleared.
  bool ccdSphereVsStatic(u32 idx, CcdSweepHit* out) const;

  // ---- TGS warm-start impulses. Caller can read for debug.
  void rememberContactPair(u32 a, u32 b, f64 normalImpulse);
  f64 lookupContactImpulse(u32 a, u32 b) const;

private:
  PhysicsWorld world_;
  std::vector<std::pair<ContactPairKey, f64>> warmStartImpulses_;
};

}  // namespace kimia::street
