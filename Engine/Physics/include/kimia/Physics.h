#pragma once

#include <kimia/Time.h>
#include <kimia/Types.h>
#include <kimia/Vec.h>

#include <map>
#include <utility>
#include <vector>

namespace kimia {

// Gravity magnitude; pulls along -Y (acceleration vector is (0, -kGravity, 0)).
inline constexpr f64 kGravity = 9.81;

// Impacts slower than this (m/s) settle instead of bouncing: a body resting
// on a surface stops instead of micro-bouncing forever with high restitution.
inline constexpr f64 kContactRestitutionThreshold = 0.5;

// Dynamic sphere body. Semi-implicit (symplectic) Euler integration.
struct SphereBody {
  Vec3 position{0.0, 0.0, 0.0};
  Vec3 velocity{0.0, 0.0, 0.0};
  f64 radius = 0.12;
  f64 mass = 1.0;  // two-way impacts (e.g. ball vs dynamic box) use mass ratio
  f64 restitution = 0.40;
  f64 friction = 0.40;
  f64 rollingFriction = 0.22;
  // How strongly the world wind pushes this body while it is airborne
  // (0 = immune, 1 = full). A heavy accurate ball can be tuned below 1.
  f64 windFactor = 1.0;
  // Spin, in radians per second, as an axis-angle vector (stage 23). A ball
  // spinning about the Y axis curls sideways; about X it dips or floats.
  // Only the Magnus force reads it — the renderer does not roll the ball.
  Vec3 spin{0.0, 0.0, 0.0};
  // How much Magnus lift this ball gets (0 = immune). A heavy accurate ball
  // curls less than a light fantasy one.
  f64 magnusFactor = 1.0;
  u32 collisionCount = 0U;  // contacts resolved during the last step
};

// Magnus: F/m = kMagnusCoefficient * (spin x velocity). Tuned so a 6 m/s
// shot with 12 rad/s of side spin bends about a third of a metre over a
// 10 m flight — a visible curl, not a boomerang.
inline constexpr f64 kMagnusCoefficient = 0.06;

// A soaked surface keeps this share of its grip (see setWetness).
inline constexpr f64 kWetMinGrip = 0.35;

// Spin bleeds away in flight with this time constant (per second), and a
// ball that touches down loses most of it at once: grass kills spin.
inline constexpr f64 kSpinAirDecay = 0.35;
inline constexpr f64 kSpinGroundKeep = 0.25;

// Dynamic axis-aligned box body (a crate): falls, slides, stacks and gets
// knocked around. No rotation — boxes stay axis-aligned. Semi-implicit
// Euler like the sphere; contacts are solved with iterative sequential
// impulses plus positional correction.
struct DynamicBox {
  Vec3 position{0.0, 0.0, 0.0};
  Vec3 halfExtents{0.5, 0.5, 0.5};
  Vec3 velocity{0.0, 0.0, 0.0};
  f64 mass = 1.0;
  f64 restitution = 0.25;
  f64 friction = 0.50;
  f64 rollingFriction = 0.05;  // extra tangential deceleration while sliding
  u32 collisionCount = 0U;     // contacts resolved during the last step
};

// Static plane at y = const (normal +Y).
struct StaticPlane {
  f64 y = 0.0;
};

// Static axis-aligned box.
struct StaticBox {
  Vec3 center{0.0, 0.0, 0.0};
  Vec3 halfExtents{0.5, 0.5, 0.5};
};

// How high a grounded character steps in one move: a kerb, a stair riser, the
// lip of a concrete block. Without it the only way over a 25 cm kerb is to
// jump, and a player walking the street pitch stops dead at it. The move is
// the classic three phases — up, across, drop back down — and it only commits
// if it lands the character further along than the plain wall-slide did, so it
// can never cost movement. It runs while onGround only: in mid-air a lump of
// concrete is a wall, which is what makes jumping feel like it has weight.
inline constexpr f64 kCharacterStepHeight = 0.35;

// How much of a character's foot must be over a surface for that surface to
// hold it up. The obvious rule — "the character's centre must be over the box"
// — cannot express stepping onto a kerb at all: the character reaches the kerb
// face with its centre 30 cm short of it, and a box can only advance a few
// millimetres per frame, so it would never be "on top". A real controller asks
// its physics engine "what is under my foot shape", and that is what this is:
// support is a footprint overlap, and any overlap at least this big counts.
inline constexpr f64 kCharacterGroundOverlap = 0.01;

// Kinematic character: an axis-aligned capsule proxy the caller drives.
// Gravity owns the vertical velocity; the caller supplies the desired
// horizontal velocity. moveCharacter() collides and slides the body along
// static planes/boxes and dynamic boxes, lands it on top faces (onGround)
// and bumps its head on ceilings. Dynamic boxes are pushed by the caller
// layer, not here — here they are solid obstacles.
struct CharacterBody {
  Vec3 position{0.0, 0.0, 0.0};  // center; feet at position.y - halfExtents.y
  Vec3 halfExtents{0.3, 0.5, 0.3};
  Vec3 velocity{0.0, 0.0, 0.0};
  bool onGround = false;
  u32 collisionCount = 0U;  // faces touched during the last move
  // Which side this character plays for (stage 21). 0 = no team (the lone
  // player of a sandbox world); 1 and 2 are the two sides of a match. The
  // physics layer only carries the number — the rules live in the game.
  u32 team = 0U;
  // How high this character can step in one move (phase 4). See
  // kCharacterStepHeight; 0 restores exactly the pre-step behaviour, which is
  // what the older tests and any caller that wants a pure wall-slide ask for.
  f64 stepHeight = kCharacterStepHeight;
};


// Falls faster than this are clamped so a slow frame cannot tunnel through
// a one-unit obstacle (6 m/s * 0.1 s = 0.6 m per host frame).
inline constexpr f64 kMaxCharacterFallSpeed = 6.0;

// Every world has a character with this id: the one the player drives.
inline constexpr u32 kPrimaryCharacter = 1U;

// --- Wind (stage 20.5-b2) ---
//
// A constant horizontal breeze blowing over the whole world. It is an
// ACCELERATION (m/s^2) applied to a dynamic sphere on every fixed step,
// exactly like gravity but sideways, scaled by the body's windFactor.
//
// Wind pushes a ball that is MOVING — in the air (a lofted football) or
// rolling along the ground (a putt drifting off line). A ball that has come
// to REST is immune: friction holds it, so a breeze can never creep a still
// ball across the course forever. On the ground the push is scaled by
// kWindGroundFactor, because the turf takes most of it.
//
// Wind is deterministic: the same wind and the same shot always land on the
// same spot, at any host frame rate.
//
// The vertical component is ignored: wind is horizontal by definition.
struct Wind {
  Vec3 acceleration{0.0, 0.0, 0.0};  // m/s^2, horizontal (y is ignored)

  bool active() const { return acceleration.x != 0.0 || acceleration.z != 0.0; }
  f64 speed() const;      // magnitude of the horizontal acceleration
  f64 direction() const;  // radians, 0 = blowing toward -Z, like the aim yaw
};

// A wind stronger than this is refused (clamped): beyond it a shot can no
// longer be aimed and the game stops being a game.
inline constexpr f64 kMaxWindAcceleration = 20.0;

// A ball slower than this (m/s) counts as at rest and ignores the wind.
inline constexpr f64 kWindRestSpeed = 0.05;
// How much of the wind reaches a ball that is touching the ground. The
// ground push is additionally capped at the friction deceleration the turf
// is already supplying, so no gale can accelerate a rolling ball for ever.
inline constexpr f64 kWindGroundFactor = 0.35;
// The hard ceiling on the ground push, as a fraction of the friction the
// surface supplies. Strictly below 1 so friction always wins in the end and
// a wind-blown ball comes to a stop instead of drifting for ever.
inline constexpr f64 kWindGroundGrip = 0.5;

// --- Broad phase (phase 4) ---
//
// Finding which dynamic bodies can possibly touch used to mean testing every
// pair: with N bodies that is N(N-1)/2 narrow-phase tests, and the solver
// re-derives them 21 times per fixed step (20 impulse iterations plus the
// friction pass). The broad phase answers "what is near this body" instead, so
// the work follows the local density rather than the body count.
//
// What it is: SWEEP AND PRUNE along X. Bodies are sorted by the low edge of
// their AABB; walking that order, a body is only compared with the ones whose
// low X edge is still behind its own high X edge, and each of those is then
// checked on Y and Z. A pair that survives is offered to the narrow phase.
//
// Why not a uniform grid, the more obvious choice: it was written first and
// measured (Tools/src/kimia_bench_physics.cpp). On a scattered field of 1000
// bodies it won big, but on the scene that matters for a street pitch — 100
// crates stacked in one heap — it came out slower than the linear scan it was
// meant to replace: every crate straddles several cells, so the hashing per
// body per collect bought fewer pair tests than it cost. Sweep and prune needs
// one sort of N and no hashing at all, and it beats the linear scan on both
// scenes. The numbers are in Documentation/Physics.md.
//
// The rule this whole mechanism is built on: the broad phase may only REJECT
// pairs the narrow phase would have rejected anyway. It never invents a
// contact and never drops one, so the contact list — and therefore the
// simulation — is unchanged, and setBroadPhaseEnabled(false) keeps the linear
// scan available as the reference the broad phase is measured against
// (Tests/src/PhysicsTests.cpp runs both and demands bit-identical bodies).
inline constexpr usize kBroadPhaseMinBodies = 16U;

// Builds a wind from a speed (m/s^2) and a direction (radians, 0 = toward -Z,
// matching WorldEditor::aimYaw). The speed is clamped to [0, kMaxWind...].
Wind makeWind(f64 speed, f64 direction);

// --- Surface material (phase 4) ---
//
// What the ground is MADE of, as far as the simulation is concerned: two
// multipliers over the body's own friction and restitution.
//
//   grip         scales how hard the ground holds a ball (asphalt 0.6 = a ball
//                rolls much further on it than on grass; sand 2.6 = it dies).
//   restitution  scales the bounce off the ground (metal 1.45 = lively,
//                sand 0.55 = a thud).
//
// The default is exactly {1.0, 1.0}: a world that never sets a material is
// bit-for-bit the world this engine had before materials existed, and the
// multipliers themselves are the only place the material shows up — the
// per-body friction and restitution keep owning everything else (so a heavy
// crate still lands hard on grass).
//
// Content names the material (grass / asphalt / concrete / metal / wood /
// rubber / sand) — see GameProfile.h — and hands the numbers down here through
// setSurfaceMaterial, so Physics stays free of game nouns and Profile stays
// free of physics.
struct SurfaceMaterial {
  f64 grip = 1.0;
  f64 restitution = 1.0;
};

// Fixed-timestep physics world: dynamic spheres and dynamic boxes vs static
// planes and AABBs, plus dynamic-vs-dynamic pairs (sphere-sphere, sphere-box,
// box-box). Fixed dt = 1/120 s; host-rate advance() uses an accumulator with
// a step cap (default 5) so a slow frame cannot spiral. Bodies are referenced
// by 1-based ids (0 is null); ids are never reused.
class PhysicsWorld {
public:
  explicit PhysicsWorld(f64 fixedDt = 1.0 / 120.0, u32 maxStepsPerFrame = 5U);

  u32 addSphere(const SphereBody& body);
  bool removeSphere(u32 id);
  SphereBody* sphere(u32 id);
  const SphereBody* sphere(u32 id) const;

  u32 addDynamicBox(const DynamicBox& body);
  bool removeDynamicBox(u32 id);
  DynamicBox* dynamicBox(u32 id);
  const DynamicBox* dynamicBox(u32 id) const;

  u32 addPlane(f64 y);
  u32 addBox(const Vec3& center, const Vec3& halfExtents);

  // --- Raycasting (stage 30) ---
  //
  // A shot is a ray, not a projectile: at rifle speed a bullet crosses a
  // 40 m arena in a few milliseconds, so simulating its flight would just
  // be an expensive way of drawing a straight line. Everything a shooter
  // needs is "what does this line hit first".
  struct RayHit {
    bool hit = false;
    f64 distance = 0.0;      // along the ray, in meters
    Vec3 point{0.0, 0.0, 0.0};
    Vec3 normal{0.0, 0.0, 0.0};
    // What was struck. Exactly one of these is set when `hit` is true.
    u32 character = 0U;  // character id, 0 = not a character
    u32 box = 0U;        // static box id, 0 = not a box
    bool ground = false;
  };

  // Casts `direction` (need not be normalised) from `origin` up to
  // `maxDistance`, returning the NEAREST hit. `ignoreCharacter` skips the
  // shooter so nobody shoots themselves in the foot.
  RayHit raycast(const Vec3& origin, const Vec3& direction, f64 maxDistance,
                 u32 ignoreCharacter = 0U) const;

  // --- Characters (stage 21: N of them) ---
  //
  // Characters have their OWN 1-based id space (they are kinematic, not
  // solver bodies, and keeping them separate leaves every sphere/box id
  // exactly where it was). Ids are never reused. Character 1 always
  // exists, so the single-character API below keeps working unchanged:
  // character() is character 1, and a world that never adds another one
  // behaves exactly as it did before.
  u32 addCharacter(const CharacterBody& body);
  bool removeCharacter(u32 id);
  CharacterBody* characterById(u32 id);
  const CharacterBody* characterById(u32 id) const;
  usize characterCount() const { return characters_.size(); }
  // The character ids in ascending (creation) order — a deterministic walk
  // for the game layer and for serialization.
  std::vector<u32> characterIds() const;

  // The first character (id kPrimaryCharacter). The one-player API.
  CharacterBody* character() { return characterById(kPrimaryCharacter); }
  const CharacterBody* character() const { return characterById(kPrimaryCharacter); }

  // The world wind (see Wind). Off by default, so every existing world and
  // every existing test behaves exactly as before.
  void setWind(const Wind& wind) { wind_ = wind; }
  const Wind& wind() const { return wind_; }

  // Wetness (stage 24): 0 = dry, 1 = as slick as this engine ever gets. A
  // wet surface keeps kWetMinGrip of its friction, so the ball runs on
  // further but never slides for ever. Dry (the default) is bit-identical
  // to the engine before this existed.
  void setWetness(f64 wetness);
  // The material the ground is made of. Two multipliers, see SurfaceMaterial:
  // the default {1.0, 1.0} is the pre-material behaviour exactly.
  void setSurfaceMaterial(const SurfaceMaterial& material) { surface_ = material; }
  const SurfaceMaterial& surfaceMaterial() const { return surface_; }
  f64 wetness() const { return wetness_; }
  // The multiplier wetness applies to contact friction: 1 when dry.
  f64 gripFactor() const;

  // Teleports the character to `position`, zeroing velocity and ground state.
  void resetCharacter(const Vec3& position);
  void resetCharacter(u32 id, const Vec3& position);

  // Moves the character for dt seconds toward the desired horizontal
  // velocity (the y component is ignored — gravity owns the vertical).
  // Characters are solid to each other: a mover is blocked by, and slides
  // along, every OTHER character as well as the level geometry.
  void moveCharacter(f64 dt, const Vec3& desiredVelocity);
  void moveCharacter(u32 id, f64 dt, const Vec3& desiredVelocity);

  // Starts a jump of the given height (meters, feet apex) when the character
  // stands on something: v = sqrt(2 g h). Returns true when the jump began.
  bool characterJump(f64 height);
  bool characterJump(u32 id, f64 height);
  // The same jump asked for as a take-off speed instead of a height, for a
  // caller that owns the number in m/s (the character motor). One formula
  // still decides how high a jump of a given speed goes: the physics.
  bool characterJumpSpeed(f64 takeOffSpeed);
  bool characterJumpSpeed(u32 id, f64 takeOffSpeed);

  // Highest Y (starting from center.y, capped at maxHeight) at which a sphere
  // of this radius at (center.x, ?, center.z) does NOT strictly overlap any
  // static or dynamic box. Used to spawn a ball safely above objects that
  // were placed on its spawn point. Returns the input y when nothing overlaps.
  f64 resolveSpawnHeight(const Vec3& center, f64 radius, f64 maxHeight) const;

  void clear();

  // One fixed step (fixedDt() seconds).
  void step();

  // Host-rate advance: accumulator-fed, capped at maxStepsPerFrame steps.
  // Returns how many fixed steps ran this frame.
  u32 advance(f64 hostSeconds);

  // --- Broad phase statistics (phase 4) ---
  //
  // How much work the contact search actually did. "The broad phase works" is
  // a claim; a pair count before and after is a measurement. Candidate pairs
  // are what the broad phase offered the narrow phase; pair tests are the exact
  // tests performed (those candidates plus the parts that are still brute
  // force: dynamic-vs-static and sphere-vs-plane).
  struct Stats {
    u64 steps = 0U;
    u64 candidatePairs = 0U;      // pairs the broad phase offered
    u64 pairTests = 0U;           // narrow-phase tests actually run
    u64 broadPhaseRebuilds = 0U;  // broad-phase sorts (one per collect)
  };
  const Stats& stats() const { return stats_; }
  void resetStats() { stats_ = Stats{}; }

  // Sweep and prune (the default) or the linear scan it replaced. Both produce
  // the same simulation; the switch exists so that equivalence is a test, and
  // so a pathological scene can be compared against the reference.
  void setBroadPhaseEnabled(bool enabled) { broadPhaseEnabled_ = enabled; }
  bool broadPhaseEnabled() const { return broadPhaseEnabled_; }

  f64 fixedDt() const { return fixedDt_; }
  f64 time() const { return time_; }
  u64 stepCount() const { return steps_; }
  usize sphereCount() const { return spheres_.size(); }
  usize dynamicBoxCount() const { return dynamicBoxes_.size(); }
  usize planeCount() const { return planes_.size(); }
  usize boxCount() const { return boxes_.size(); }

private:
  struct Contact {
    bool sphereA = false;  // A is a dynamic sphere (else a dynamic box)
    bool sphereB = false;  // B is a dynamic sphere; false = dynamic box
    bool staticB = false;  // B is a static plane or box (A is never static)
    bool ground = false;   // B is the ground plane, so the surface material applies
    bool embedded = false; // sphere center inside a box: position-only fix
    // NOTE: collectContacts builds these positionally. The order above — and
    // `ground` sitting before `embedded` — is what the literals there assume.
    u32 idA = 0U;
    u32 idB = 0U;
    Vec3 normal{0.0, 0.0, 0.0};  // points from A toward B
    f64 penetration = 0.0;
    f64 restitution = 0.0;
  };

  void collectContacts(std::vector<Contact>& contacts) const;
  // Fills candidates_ with the dynamic-dynamic pairs whose AABBs overlap
  // (ascending by id, each pair offered once). Returns false when the sweep was
  // not worth running, leaving candidates_ empty; the caller then walks every
  // pair — which is what this did before the broad phase existed.
  bool collectDynamicPairs() const;
  void addPair(u32 idA, u32 idB) const;
  void resolvePair(const Contact& contact, bool countContacts);
  void applyPairFriction(const Contact& contact);
  bool characterSupported(const CharacterBody& character, u32 selfId) const;
  // Character-controller helpers (phase 4). `characterBlocked` answers "would
  // the box be inside anything solid at this position?"; `characterGroundBelow`
  // looks for the highest surface under the footprint, at most maxDrop below
  // the feet, and reports where those feet would rest. `stepUp` is the
  // up-across-down attempt described at kCharacterStepHeight.
  bool characterBlocked(const CharacterBody& character, u32 selfId, const Vec3& position) const;
  bool characterGroundBelow(const CharacterBody& character, u32 selfId, const Vec3& position,
                            f64 maxDrop, f64& outFeet) const;
  // `startPosition` is where the move began (before the wall-slide), so the
  // step attempt replays the same dt from the same place rather than adding a
  // second helping of movement on top of the slide.
  bool characterStepUp(CharacterBody& character, u32 selfId, f64 dt, const Vec3& desiredVelocity,
                       const Vec3& startPosition) const;
  void characterCollideAndSlide(CharacterBody& character, u32 selfId, f64 dt) const;
  void characterResolveVertical(CharacterBody& character, u32 selfId, f64 dt) const;

  f64 fixedDt_;
  FixedTimeStep accumulator_;
  std::map<u32, SphereBody> spheres_;
  std::map<u32, DynamicBox> dynamicBoxes_;
  std::map<u32, StaticPlane> planes_;
  std::map<u32, StaticBox> boxes_;
  std::map<u32, CharacterBody> characters_;
  u32 nextCharacterId_ = kPrimaryCharacter;
  Wind wind_;
  f64 wetness_ = 0.0;
  SurfaceMaterial surface_;
  u32 nextId_ = 1U;
  f64 time_ = 0.0;
  u64 steps_ = 0U;

  bool broadPhaseEnabled_ = true;

  // The sweep is a cache rebuilt from the bodies on every collect, never state:
  // `mutable` for the same reason Scene's name index is (a const query may fill
  // it, and it can always be thrown away and rebuilt). The containers keep their
  // capacity across collects, so a steady frame does not allocate here. Same for
  // the counters, which exist to be reported, not to change behaviour.
  struct Proxy {  // one dynamic body's AABB, in ascending id order
    u32 id = 0U;
    bool sphere = false;
    Vec3 center{0.0, 0.0, 0.0};
    Vec3 half{0.0, 0.0, 0.0};
  };
  mutable std::vector<Proxy> proxies_;
  mutable std::vector<u32> sweepOrder_;  // proxy indices, sorted by low X edge
  mutable std::vector<std::pair<u32, u32>> candidates_;  // ascending by id
  mutable Stats stats_;
};

}  // namespace kimia
