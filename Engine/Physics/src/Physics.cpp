#include <kimia/Physics.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace kimia {

namespace {

constexpr f64 kEpsilon = 1e-12;

// Sequential-impulse solver tuning: 20 iterations of fresh contact
// resolution per step. Positional correction removes a fraction of the
// penetration per iteration, so stacked bodies converge without jitter;
// 20 iterations propagate the weight of a stack through the chain (each
// iteration halves the transferred residual, ~2^-20 of g*dt at the end).
constexpr u32 kSolverIterations = 20U;
constexpr f64 kPositionCorrection = 0.8;

f64 clampValue(f64 value, f64 lo, f64 hi) { return value < lo ? lo : (value > hi ? hi : value); }

// Closest point of `point` to the AABB(center, halfExtents).
Vec3 closestPointOnBox(const Vec3& point, const Vec3& center, const Vec3& halfExtents) {
  return Vec3{
      clampValue(point.x, center.x - halfExtents.x, center.x + halfExtents.x),
      clampValue(point.y, center.y - halfExtents.y, center.y + halfExtents.y),
      clampValue(point.z, center.z - halfExtents.z, center.z + halfExtents.z),
  };
}

// Sphere-vs-box contact. On success `normal` points from the sphere toward
// the box and `penetration` is the overlap depth. `embedded` is true when the
// sphere CENTER is inside the box (deep overlap): such contacts only correct
// position (exit through the top), never apply a velocity impulse.
bool sphereBoxContact(const Vec3& spherePos, f64 radius, const Vec3& boxPos, const Vec3& half,
                      Vec3& normal, f64& penetration, bool& embedded) {
  const Vec3 closest = closestPointOnBox(spherePos, boxPos, half);
  const Vec3 delta = spherePos - closest;  // points from the box toward the sphere
  const f64 distanceSquared = delta.lengthSquared();
  if (distanceSquared > radius * radius) return false;  // touching (== r^2) counts as contact
  embedded = false;
  if (distanceSquared > kEpsilon) {
    const f64 distance = std::sqrt(distanceSquared);
    normal = (delta / distance) * -1.0;  // flip: from the sphere toward the box
    penetration = radius - distance;
  } else {
    // Sphere center inside the box: exit UP through the top face. Exiting
    // along the nearest face would push the sphere down through the floor
    // plane when a box sits on the ground (the plane and the box then fight
    // and the sphere sinks), so the center-inside case always pops the
    // sphere out on top of the box. The solver moves the sphere along
    // -normal, so normal points back down into the box here.
    normal = Vec3{0.0, -1.0, 0.0};
    penetration = (boxPos.y + half.y - spherePos.y) + radius;
    embedded = true;
  }
  return true;
}

// Axis-aligned box-vs-box contact. On success `normal` points from A toward B.
bool boxBoxContact(const Vec3& posA, const Vec3& halfA, const Vec3& posB, const Vec3& halfB,
                   Vec3& normal, f64& penetration) {
  const f64 dx = halfA.x + halfB.x - std::abs(posB.x - posA.x);
  const f64 dy = halfA.y + halfB.y - std::abs(posB.y - posA.y);
  const f64 dz = halfA.z + halfB.z - std::abs(posB.z - posA.z);
  if (dx <= 0.0 || dy <= 0.0 || dz <= 0.0) return false;
  const Vec3 delta = posB - posA;
  if (dx <= dy && dx <= dz) {
    normal = Vec3{delta.x >= 0.0 ? 1.0 : -1.0, 0.0, 0.0};
    penetration = dx;
  } else if (dy <= dz) {
    normal = Vec3{0.0, delta.y >= 0.0 ? 1.0 : -1.0, 0.0};
    penetration = dy;
  } else {
    normal = Vec3{0.0, 0.0, delta.z >= 0.0 ? 1.0 : -1.0};
    penetration = dz;
  }
  return true;
}

// Constant-force tangential damping: decelerates by factor * g per second,
// never overshooting past zero (the ball/crate friction model).
void dampTangential(Vec3& velocity, const Vec3& normal, f64 factor, f64 dt) {
  const Vec3 tangential = velocity - normal * kimia::dot(velocity, normal);
  const f64 speed = tangential.length();
  if (speed <= 0.0) return;
  const f64 deceleration = factor * kGravity * dt;
  const f64 scale = speed > deceleration ? 1.0 - deceleration / speed : 0.0;
  velocity -= tangential * (1.0 - scale);
}

}  // namespace

PhysicsWorld::PhysicsWorld(f64 fixedDt, u32 maxStepsPerFrame)
    : fixedDt_(fixedDt), accumulator_(fixedDt, maxStepsPerFrame) {
  addCharacter(CharacterBody{});  // character 1: the player, always present
}

u32 PhysicsWorld::addSphere(const SphereBody& body) {
  const u32 id = nextId_;
  ++nextId_;
  spheres_.emplace(id, body);
  return id;
}

bool PhysicsWorld::removeSphere(u32 id) { return spheres_.erase(id) > 0U; }

SphereBody* PhysicsWorld::sphere(u32 id) {
  const auto found = spheres_.find(id);
  return found == spheres_.end() ? nullptr : &found->second;
}

const SphereBody* PhysicsWorld::sphere(u32 id) const {
  const auto found = spheres_.find(id);
  return found == spheres_.end() ? nullptr : &found->second;
}

u32 PhysicsWorld::addDynamicBox(const DynamicBox& body) {
  const u32 id = nextId_;
  ++nextId_;
  dynamicBoxes_.emplace(id, body);
  return id;
}

bool PhysicsWorld::removeDynamicBox(u32 id) { return dynamicBoxes_.erase(id) > 0U; }

DynamicBox* PhysicsWorld::dynamicBox(u32 id) {
  const auto found = dynamicBoxes_.find(id);
  return found == dynamicBoxes_.end() ? nullptr : &found->second;
}

const DynamicBox* PhysicsWorld::dynamicBox(u32 id) const {
  const auto found = dynamicBoxes_.find(id);
  return found == dynamicBoxes_.end() ? nullptr : &found->second;
}

u32 PhysicsWorld::addPlane(f64 y) {
  const u32 id = nextId_;
  ++nextId_;
  planes_.emplace(id, StaticPlane{y});
  return id;
}

u32 PhysicsWorld::addBox(const Vec3& center, const Vec3& halfExtents) {
  const u32 id = nextId_;
  ++nextId_;
  boxes_.emplace(id, StaticBox{center, halfExtents});
  return id;
}

void PhysicsWorld::clear() {
  spheres_.clear();
  dynamicBoxes_.clear();
  planes_.clear();
  boxes_.clear();
  // Characters: the extra players go away, but player 1 is KEPT as it is.
  // clear() rebuilds the level, not the player — the game positions the
  // player itself (resetCharacter) and used to rely on it surviving here.
  const CharacterBody primary = characters_.count(kPrimaryCharacter) > 0U
                                    ? characters_.at(kPrimaryCharacter)
                                    : CharacterBody{};
  characters_.clear();
  nextCharacterId_ = kPrimaryCharacter;
  addCharacter(primary);
  time_ = 0.0;
  steps_ = 0U;
}


//
// Sweep and prune along X. The bodies are sorted by the low edge of their AABB
// and swept in that order: while a later body's low X edge is still behind the
// current body's high X edge the two can touch, and a Y/Z check decides whether
// they really can. Only AABB overlaps reach the narrow phase, which has the
// final say — so the contact list is exactly what the linear scan produced.
//
// The first version of this was a uniform grid. It won on a scattered field and
// LOST on a heap of crates (every crate straddles several cells, so the hashing
// cost more than the pair tests it saved); the measurement is in
// Documentation/Physics.md and Tools/src/kimia_bench_physics.cpp. Sweeping needs
// one sort of N and no hashing at all, and it beats the linear scan on both.

void PhysicsWorld::addPair(u32 idA, u32 idB) const {
  // Ascending ids, always: the contact list is built from these pairs in this
  // order, and the sequential-impulse solver is sensitive to it.
  if (idA == idB) return;
  candidates_.push_back(idA < idB ? std::make_pair(idA, idB) : std::make_pair(idB, idA));
}

bool PhysicsWorld::collectDynamicPairs() const {
  candidates_.clear();
  const usize bodyCount = spheres_.size() + dynamicBoxes_.size();
  if (!broadPhaseEnabled_ || bodyCount < kBroadPhaseMinBodies) return false;
  ++stats_.broadPhaseRebuilds;

  // One proxy per dynamic body, in ascending id order (the two maps share one
  // id space, so this is also the order the maps would have produced).
  proxies_.clear();
  proxies_.reserve(bodyCount);
  for (const auto& pair : spheres_) {
    Proxy proxy;
    proxy.id = pair.first;
    proxy.sphere = true;
    proxy.center = pair.second.position;
    proxy.half = Vec3{pair.second.radius, pair.second.radius, pair.second.radius};
    proxies_.push_back(proxy);
  }
  for (const auto& pair : dynamicBoxes_) {
    Proxy proxy;
    proxy.id = pair.first;
    proxy.sphere = false;
    proxy.center = pair.second.position;
    proxy.half = pair.second.halfExtents;
    proxies_.push_back(proxy);
  }
  std::sort(proxies_.begin(), proxies_.end(),
            [](const Proxy& a, const Proxy& b) { return a.id < b.id; });

  sweepOrder_.resize(proxies_.size());
  for (usize i = 0; i < sweepOrder_.size(); ++i) sweepOrder_[i] = static_cast<u32>(i);
  // Sort by the low X edge. Ties go by proxy index, which is id order: two
  // bodies in the same place must produce the same sweep every single frame, or
  // the simulation stops being reproducible.
  std::sort(sweepOrder_.begin(), sweepOrder_.end(), [this](u32 a, u32 b) {
    const f64 lowA = proxies_[a].center.x - proxies_[a].half.x;
    const f64 lowB = proxies_[b].center.x - proxies_[b].half.x;
    if (lowA != lowB) return lowA < lowB;
    return a < b;
  });

  for (usize slot = 0; slot < sweepOrder_.size(); ++slot) {
    const Proxy& a = proxies_[sweepOrder_[slot]];
    const f64 aHighX = a.center.x + a.half.x;
    const f64 aLowX = a.center.x - a.half.x;
    static_cast<void>(aLowX);
    for (usize ahead = slot + 1U; ahead < sweepOrder_.size(); ++ahead) {
      const Proxy& b = proxies_[sweepOrder_[ahead]];
      if (b.center.x - b.half.x > aHighX) break;  // sorted: nobody further can reach
      // Y, Z, then X exactly: an AABB that overlaps on one axis only is not a
      // candidate, and the narrow phase would have thrown it away.
      if (std::abs(b.center.y - a.center.y) > a.half.y + b.half.y) continue;
      if (std::abs(b.center.z - a.center.z) > a.half.z + b.half.z) continue;
      if (std::abs(b.center.x - a.center.x) > a.half.x + b.half.x) continue;
      addPair(a.id, b.id);
    }
  }
  // The sweep emits every pair once, in X order; the contact list needs them in
  // id order (see addPair), so one sort puts them there. Sorting here is cheap:
  // the list holds the pairs that really can touch, not all N(N-1)/2 of them.
  std::sort(candidates_.begin(), candidates_.end());
  stats_.candidatePairs += static_cast<u64>(candidates_.size());
  return true;
}

void PhysicsWorld::collectContacts(std::vector<Contact>& contacts) const {
  contacts.clear();
  u64 tests = 0U;

  // Sphere vs plane.
  for (const auto& spherePair : spheres_) {
    const SphereBody& sphere = spherePair.second;
    for (const auto& planePair : planes_) {
      ++tests;
      const f64 distance = sphere.position.y - sphere.radius - planePair.second.y;
      if (distance <= 0.0) {  // touching counts: resting bodies stay damped
        contacts.push_back(Contact{true, false, true, true, false, spherePair.first, planePair.first, Vec3{0.0, -1.0, 0.0},
                                   -distance, sphere.restitution * surface_.restitution});
      }
    }
  }

  // Sphere vs static box.
  for (const auto& spherePair : spheres_) {
    const SphereBody& sphere = spherePair.second;
    for (const auto& boxPair : boxes_) {
      ++tests;
      Vec3 normal;
      f64 penetration = 0.0;
      bool embedded = false;
      if (sphereBoxContact(sphere.position, sphere.radius, boxPair.second.center, boxPair.second.halfExtents,
                           normal, penetration, embedded)) {
        contacts.push_back(Contact{true, false, true, false, embedded, spherePair.first, boxPair.first, normal,
                                   penetration, sphere.restitution});
      }
    }
  }

  // --- Dynamic vs dynamic ---
  //
  // The broad phase (phase 4) offers the pairs whose AABBs overlap; when it does
  // not run (a small world, or the linear reference path) every pair is tested,
  // which is what this used to do always. Either way the NARROW phase decides,
  // so the contact list is identical — the sweep only skips pairs that could not
  // have touched. The three families are walked in their own passes because the
  // ORDER of contacts decides how sequential impulses settle a stack:
  // sphere-sphere, then box-box, then sphere-box, each ascending by id.
  const bool candidateDriven = collectDynamicPairs();

  // Sphere vs sphere.
  const auto sphereSphereContact = [this, &contacts](u32 idA, u32 idB) {
    const SphereBody* a = sphere(idA);
    const SphereBody* b = sphere(idB);
    if (a == nullptr || b == nullptr) return;
    const Vec3 delta = b->position - a->position;
    const f64 radii = a->radius + b->radius;
    const f64 distanceSquared = delta.lengthSquared();
    if (distanceSquared >= radii * radii) return;
    Vec3 normal{1.0, 0.0, 0.0};
    f64 penetration = radii;
    if (distanceSquared > kEpsilon) {
      const f64 distance = std::sqrt(distanceSquared);
      normal = delta / distance;
      penetration = radii - distance;
    }
    const f64 restitution = std::max(a->restitution, b->restitution);
    contacts.push_back(Contact{true, true, false, false, false, idA, idB, normal, penetration, restitution});
  };
  if (candidateDriven) {
    for (const auto& pair : candidates_) {
      if (!sphere(pair.first) || !sphere(pair.second)) continue;
      ++tests;
      sphereSphereContact(pair.first, pair.second);
    }
  } else {
    for (auto a = spheres_.begin(); a != spheres_.end(); ++a) {
      for (auto b = std::next(a); b != spheres_.end(); ++b) {
        ++tests;
        sphereSphereContact(a->first, b->first);
      }
    }
  }

  // Dynamic box vs plane.
  for (const auto& boxPair : dynamicBoxes_) {
    const DynamicBox& box = boxPair.second;
    for (const auto& planePair : planes_) {
      ++tests;
      const f64 distance = box.position.y - box.halfExtents.y - planePair.second.y;
      if (distance <= 0.0) {  // touching counts: resting bodies stay damped
        contacts.push_back(Contact{false, false, true, true, false, boxPair.first, planePair.first, Vec3{0.0, -1.0, 0.0},
                                   -distance, box.restitution * surface_.restitution});
      }
    }
  }

  // Dynamic box vs static box.
  for (const auto& boxPair : dynamicBoxes_) {
    const DynamicBox& box = boxPair.second;
    for (const auto& staticPair : boxes_) {
      ++tests;
      Vec3 normal;
      f64 penetration = 0.0;
      if (boxBoxContact(box.position, box.halfExtents, staticPair.second.center, staticPair.second.halfExtents,
                        normal, penetration)) {
        contacts.push_back(
            Contact{false, false, true, false, false, boxPair.first, staticPair.first, normal, penetration,
                    box.restitution});
      }
    }
  }

  // Dynamic box vs dynamic box.
  const auto boxBoxDynamicContact = [this, &contacts](u32 idA, u32 idB) {
    const DynamicBox* a = dynamicBox(idA);
    const DynamicBox* b = dynamicBox(idB);
    if (a == nullptr || b == nullptr) return;
    Vec3 normal;
    f64 penetration = 0.0;
    if (!boxBoxContact(a->position, a->halfExtents, b->position, b->halfExtents, normal, penetration)) return;
    const f64 restitution = std::max(a->restitution, b->restitution);
    contacts.push_back(Contact{false, false, false, false, false, idA, idB, normal, penetration, restitution});
  };
  if (candidateDriven) {
    for (const auto& pair : candidates_) {
      if (!dynamicBox(pair.first) || !dynamicBox(pair.second)) continue;
      ++tests;
      boxBoxDynamicContact(pair.first, pair.second);
    }
  } else {
    for (auto a = dynamicBoxes_.begin(); a != dynamicBoxes_.end(); ++a) {
      for (auto b = std::next(a); b != dynamicBoxes_.end(); ++b) {
        ++tests;
        boxBoxDynamicContact(a->first, b->first);
      }
    }
  }

  // Sphere vs dynamic box.
  const auto sphereBoxDynamicContact = [this, &contacts](u32 sphereId, u32 boxId) {
    const SphereBody* body = sphere(sphereId);
    const DynamicBox* box = dynamicBox(boxId);
    if (body == nullptr || box == nullptr) return;
    Vec3 normal;
    f64 penetration = 0.0;
    bool embedded = false;
    if (!sphereBoxContact(body->position, body->radius, box->position, box->halfExtents, normal,
                          penetration, embedded)) {
      return;
    }
    const f64 restitution = std::max(body->restitution, box->restitution);
    contacts.push_back(
        Contact{true, false, false, false, embedded, sphereId, boxId, normal, penetration, restitution});
  };
  if (candidateDriven) {
    for (const auto& pair : candidates_) {
      if (!sphere(pair.first) || !dynamicBox(pair.second)) continue;
      ++tests;
      sphereBoxDynamicContact(pair.first, pair.second);
    }
  } else {
    for (const auto& spherePair : spheres_) {
      for (const auto& boxPair : dynamicBoxes_) {
        ++tests;
        sphereBoxDynamicContact(spherePair.first, boxPair.first);
      }
    }
  }

  stats_.pairTests += tests;
}

void PhysicsWorld::resolvePair(const Contact& contact, bool countContacts) {
  Vec3* posA = nullptr;
  Vec3* velA = nullptr;
  f64 invA = 0.0;
  u32* countA = nullptr;
  if (contact.sphereA) {
    SphereBody* body = sphere(contact.idA);
    if (body == nullptr) return;
    posA = &body->position;
    velA = &body->velocity;
    invA = body->mass > kEpsilon ? 1.0 / body->mass : 0.0;
    countA = &body->collisionCount;
  } else {
    DynamicBox* body = dynamicBox(contact.idA);
    if (body == nullptr) return;
    posA = &body->position;
    velA = &body->velocity;
    invA = body->mass > kEpsilon ? 1.0 / body->mass : 0.0;
    countA = &body->collisionCount;
  }

  Vec3* posB = nullptr;
  Vec3* velB = nullptr;
  f64 invB = 0.0;
  u32* countB = nullptr;
  if (contact.staticB) {
    // Static: the id is a plane (planes_) or a static box (boxes_).
    if (planes_.find(contact.idB) == planes_.end() && boxes_.find(contact.idB) == boxes_.end()) return;
  } else if (contact.sphereB) {
    SphereBody* body = sphere(contact.idB);
    if (body == nullptr) return;
    posB = &body->position;
    velB = &body->velocity;
    invB = body->mass > kEpsilon ? 1.0 / body->mass : 0.0;
    countB = &body->collisionCount;
  } else {
    DynamicBox* body = dynamicBox(contact.idB);
    if (body == nullptr) return;
    posB = &body->position;
    velB = &body->velocity;
    invB = body->mass > kEpsilon ? 1.0 / body->mass : 0.0;
    countB = &body->collisionCount;
  }

  const f64 total = invA + invB;
  if (total <= kEpsilon) return;  // both effectively static: nothing to resolve

  // Positional correction: separate the bodies along the contact normal.
  const f64 correction = contact.penetration * kPositionCorrection;
  if (posA != nullptr) *posA -= contact.normal * (correction * (invA / total));
  if (posB != nullptr) *posB += contact.normal * (correction * (invB / total));

  // Normal impulse: reflect approaching motion with restitution; impacts
  // slower than the threshold settle (no micro-bouncing).
  const Vec3 velocityA = velA != nullptr ? *velA : Vec3{0.0, 0.0, 0.0};
  const Vec3 velocityB = velB != nullptr ? *velB : Vec3{0.0, 0.0, 0.0};
  const f64 approach = kimia::dot(velocityB - velocityA, contact.normal);
  if (approach < 0.0 && !contact.embedded) {
    const f64 impulse =
        (-approach >= kContactRestitutionThreshold ? -(1.0 + contact.restitution) * approach : -approach) / total;
    if (velA != nullptr) *velA -= contact.normal * (impulse * invA);
    if (velB != nullptr) *velB += contact.normal * (impulse * invB);
  }

  if (countContacts) {
    if (countA != nullptr) ++*countA;
    if (countB != nullptr) ++*countB;
  }
}

void PhysicsWorld::setWetness(f64 wetness) {
  wetness_ = wetness < 0.0 ? 0.0 : (wetness > 1.0 ? 1.0 : wetness);
}

// Dry returns exactly 1.0, so a dry world is bit-identical to one with no
// notion of weather at all.
f64 PhysicsWorld::gripFactor() const {
  if (wetness_ == 0.0) return 1.0;
  return 1.0 - (1.0 - kWetMinGrip) * wetness_;
}

void PhysicsWorld::applyPairFriction(const Contact& contact) {
  // Two independent multipliers, in this order: the weather (a wet pitch is
  // slick) and the material (asphalt slips, rubber grabs). Grass is 1.0 for
  // both, so a neutral world is unchanged to the bit.
  const f64 grip = gripFactor() * (contact.ground ? surface_.grip : 1.0);
  if (contact.sphereA) {
    SphereBody* body = sphere(contact.idA);
    if (body != nullptr) {
      dampTangential(body->velocity, contact.normal, (body->friction + body->rollingFriction) * grip, fixedDt_);
    }
  } else {
    DynamicBox* body = dynamicBox(contact.idA);
    if (body != nullptr) {
      dampTangential(body->velocity, contact.normal, (body->friction + body->rollingFriction) * grip, fixedDt_);
    }
  }
  if (!contact.staticB) {
    if (contact.sphereB) {
      SphereBody* body = sphere(contact.idB);
      if (body != nullptr) {
        dampTangential(body->velocity, contact.normal, (body->friction + body->rollingFriction) * grip, fixedDt_);
      }
    } else {
      DynamicBox* body = dynamicBox(contact.idB);
      if (body != nullptr) {
        dampTangential(body->velocity, contact.normal, (body->friction + body->rollingFriction) * grip, fixedDt_);
      }
    }
  }
}

// --- Wind ---

f64 Wind::speed() const { return std::sqrt(acceleration.x * acceleration.x + acceleration.z * acceleration.z); }

f64 Wind::direction() const {
  // Mirror of WorldEditor::aimDirection: yaw 0 points toward -Z, and yaw
  // grows toward -X, so (x, z) = (-sin, -cos) * speed.
  if (!active()) return 0.0;
  return std::atan2(-acceleration.x, -acceleration.z);
}

Wind makeWind(f64 speed, f64 direction) {
  const f64 clamped = std::max(0.0, std::min(speed, kMaxWindAcceleration));
  Wind wind;
  wind.acceleration = Vec3{-std::sin(direction) * clamped, 0.0, -std::cos(direction) * clamped};
  // Kill the denormal residue of sin/cos at the cardinal angles so an
  // "off" wind (speed 0) is exactly inactive and byte-stable.
  if (clamped == 0.0) wind.acceleration = Vec3{0.0, 0.0, 0.0};
  return wind;
}

f64 PhysicsWorld::resolveSpawnHeight(const Vec3& center, f64 radius, f64 maxHeight) const {
  f64 y = center.y;
  for (int iteration = 0; iteration < 8; ++iteration) {
    f64 raiseTo = y;
    for (const auto& boxPair : boxes_) {
      const StaticBox& box = boxPair.second;
      if (std::abs(center.x - box.center.x) >= box.halfExtents.x + radius) continue;
      if (std::abs(center.z - box.center.z) >= box.halfExtents.z + radius) continue;
      const f64 top = box.center.y + box.halfExtents.y;
      const f64 bottom = box.center.y - box.halfExtents.y;
      if (y + radius > bottom + 1e-9 && y - radius < top - 1e-9) {
        raiseTo = std::max(raiseTo, top + radius + 1e-4);
      }
    }
    for (const auto& boxPair : dynamicBoxes_) {
      const DynamicBox& box = boxPair.second;
      if (std::abs(center.x - box.position.x) >= box.halfExtents.x + radius) continue;
      if (std::abs(center.z - box.position.z) >= box.halfExtents.z + radius) continue;
      const f64 top = box.position.y + box.halfExtents.y;
      const f64 bottom = box.position.y - box.halfExtents.y;
      if (y + radius > bottom + 1e-9 && y - radius < top - 1e-9) {
        raiseTo = std::max(raiseTo, top + radius + 1e-4);
      }
    }
    if (raiseTo <= y) return y;
    if (raiseTo > maxHeight) return maxHeight;
    y = raiseTo;
  }
  return y;
}

void PhysicsWorld::step() {
  for (auto& spherePair : spheres_) {
    SphereBody& body = spherePair.second;
    // Wind pushes a MOVING ball, never a resting one (see Wind). The
    // collisionCount still holds the PREVIOUS step's contact count here (it
    // is cleared just below), which is how we know we were on the ground.
    const bool grounded = body.collisionCount != 0U;
    body.collisionCount = 0U;
    // The speed the body ARRIVED with, before this step's gravity: a ball
    // parked on the ground reads exactly zero here (the contact solver
    // zeroed it last step), which is what makes "at rest" detectable.
    const f64 arrivedHorizontal = std::sqrt(body.velocity.x * body.velocity.x + body.velocity.z * body.velocity.z);
    body.velocity.y -= kGravity * fixedDt_;
    if (body.windFactor != 0.0 && wind_.active()) {
      // A ball at rest is held by friction: the breeze can never start it
      // moving, which is what stops a still ball creeping across the course
      // forever. On the ground only the HORIZONTAL speed counts (a ball
      // settling under gravity is not "rolling").
      // Only the ground can hold a ball still — in the air there is nothing
      // to grip it, so anything airborne always catches the breeze.
      const bool resting = grounded && arrivedHorizontal <= kWindRestSpeed;
      if (!resting) {
        f64 accel = wind_.speed() * body.windFactor;
        if (grounded) {
          // The turf takes most of the breeze, and it can never supply more
          // push than the friction it is fighting — otherwise a strong
          // enough gale would accelerate a rolling ball for ever.
          // ... and never more than kWindGroundGrip of the friction the turf
          // is already supplying, so a rolling ball always still slows down
          // and stops: wind bends a putt, it never drives it for ever.
          const f64 frictionBudget = (body.friction + body.rollingFriction) * kGravity;
          accel = std::min(accel * kWindGroundFactor, frictionBudget * kWindGroundGrip);
        }
        const f64 windSpeed = wind_.speed();
        if (windSpeed > kEpsilon) {
          const f64 scale = accel * fixedDt_ / windSpeed;
          body.velocity.x += wind_.acceleration.x * scale;
          body.velocity.z += wind_.acceleration.z * scale;
        }
      }
    }
    // Magnus (stage 23): a spinning ball is pushed sideways by the air,
    // which is what bends a free kick. Only in the air — on the turf the
    // contact solver owns the ball. Spin decays as the air drags on it.
    if (body.magnusFactor != 0.0) {
      const f64 spinLength = body.spin.length();
      if (spinLength > kEpsilon) {
        if (!grounded) {
          const Vec3 magnus = cross(body.spin, body.velocity) * (kMagnusCoefficient * body.magnusFactor);
          body.velocity += magnus * fixedDt_;
          body.spin -= body.spin * (kSpinAirDecay * fixedDt_);
        } else {
          // Touching down scrubs the spin off against the ground.
          body.spin = body.spin * kSpinGroundKeep;
        }
      }
    }
    body.position += body.velocity * fixedDt_;
  }
  for (auto& boxPair : dynamicBoxes_) {
    DynamicBox& body = boxPair.second;
    body.collisionCount = 0U;
    body.velocity.y -= kGravity * fixedDt_;
    body.position += body.velocity * fixedDt_;
  }

  std::vector<Contact> contacts;
  for (u32 iteration = 0U; iteration < kSolverIterations; ++iteration) {
    collectContacts(contacts);
    for (const Contact& contact : contacts) resolvePair(contact, iteration == 0U);
  }

  // One friction pass per contact pair, after the impulses converge.
  collectContacts(contacts);
  for (const Contact& contact : contacts) applyPairFriction(contact);

  time_ += fixedDt_;
  ++steps_;
  ++stats_.steps;
}

u32 PhysicsWorld::advance(f64 hostSeconds) {
  return accumulator_.advance(hostSeconds, [this](f64) { step(); });
}

// --- Character controller ---

namespace {

// True when the two AABBs strictly overlap on all three axes.
bool characterBoxOverlaps(const Vec3& aPosition, const Vec3& aHalf, const Vec3& bPosition,
                          const Vec3& bHalf) {
  return std::abs(aPosition.x - bPosition.x) < aHalf.x + bHalf.x &&
         std::abs(aPosition.y - bPosition.y) < aHalf.y + bHalf.y &&
         std::abs(aPosition.z - bPosition.z) < aHalf.z + bHalf.z;
}

// How deep the character's box is inside `box` along one axis. <= 0 means the
// two are apart on that axis.
f64 characterAxisOverlap(const Vec3& position, const Vec3& half, const Vec3& boxCenter,
                         const Vec3& boxHalf, i32 axis) {
  if (axis == 0) return half.x + boxHalf.x - std::abs(position.x - boxCenter.x);
  if (axis == 1) return half.y + boxHalf.y - std::abs(position.y - boxCenter.y);
  return half.z + boxHalf.z - std::abs(position.z - boxCenter.z);
}

// After an axis move, push the character out of a box along that axis and
// kill the velocity on it. axis: 0 = X, 1 = Y, 2 = Z.
//
// `before` is where the character stood on that axis before this move. Only a
// penetration the move itself created is this axis' business. That guard is
// what stops phantom pushes: a character snapped exactly to a face sits a
// floating-point hair inside it (`3.0 - 2.2` is not `0.5 + 0.3`), and while
// that hair divides out of an axis it did not move on, it used to make a
// character standing on a stair tread get shoved sideways out of the next
// riser. Resolving an overlap the move did not create is the bug, not the
// precision.
void characterResolveAxis(CharacterBody& character, const Vec3& boxCenter, const Vec3& boxHalf,
                          i32 axis, f64 before) {
  if (!characterBoxOverlaps(character.position, character.halfExtents, boxCenter, boxHalf)) return;
  const f64 overlap = characterAxisOverlap(character.position, character.halfExtents, boxCenter, boxHalf, axis);
  if (overlap <= 0.0) return;
  if (overlap <= characterAxisOverlap(Vec3{before, before, before}, character.halfExtents, boxCenter, boxHalf, axis)) {
    return;  // already there before the move: not this axis' doing
  }
  f64* position = nullptr;
  f64 half = 0.0;
  f64 center = 0.0;
  f64 otherHalf = 0.0;
  f64* velocity = nullptr;
  if (axis == 0) {
    position = &character.position.x;
    half = character.halfExtents.x;
    center = boxCenter.x;
    otherHalf = boxHalf.x;
    velocity = &character.velocity.x;
  } else if (axis == 1) {
    position = &character.position.y;
    half = character.halfExtents.y;
    center = boxCenter.y;
    otherHalf = boxHalf.y;
    velocity = &character.velocity.y;
  } else {
    position = &character.position.z;
    half = character.halfExtents.z;
    center = boxCenter.z;
    otherHalf = boxHalf.z;
    velocity = &character.velocity.z;
  }
  *position = *position < center ? center - otherHalf - half : center + otherHalf + half;
  *velocity = 0.0;
  ++character.collisionCount;
}

}  // namespace

// Feet still resting on a surface? (a plane at the feet height, a box whose
// top face is under the character's center, or ANOTHER character's head —
// standing on a team-mate is legal, it just has to be stable.)
bool PhysicsWorld::characterSupported(const CharacterBody& character, u32 selfId) const {
  const f64 feet = character.position.y - character.halfExtents.y;
  for (const auto& pair : planes_) {
    if (std::abs(feet - pair.second.y) <= 1e-6) return true;
  }
  constexpr f64 kSupportTolerance = 1e-4;
  // The character's foot print, not its centre: see kCharacterGroundOverlap.
  const auto supportedByBox = [&character, feet](const Vec3& center, const Vec3& half) {
    if (std::abs(feet - (center.y + half.y)) > kSupportTolerance) return false;
    const f64 reachX = half.x + character.halfExtents.x - kCharacterGroundOverlap;
    const f64 reachZ = half.z + character.halfExtents.z - kCharacterGroundOverlap;
    if (std::abs(character.position.x - center.x) > reachX) return false;
    if (std::abs(character.position.z - center.z) > reachZ) return false;
    return true;
  };
  for (const auto& pair : boxes_) {
    if (supportedByBox(pair.second.center, pair.second.halfExtents)) return true;
  }
  for (const auto& pair : dynamicBoxes_) {
    if (supportedByBox(pair.second.position, pair.second.halfExtents)) return true;
  }
  for (const auto& pair : characters_) {
    if (pair.first == selfId) continue;
    if (supportedByBox(pair.second.position, pair.second.halfExtents)) return true;
  }
  return false;
}

// --- Characters ---

u32 PhysicsWorld::addCharacter(const CharacterBody& body) {
  const u32 id = nextCharacterId_++;
  characters_[id] = body;
  return id;
}

bool PhysicsWorld::removeCharacter(u32 id) { return characters_.erase(id) > 0U; }

CharacterBody* PhysicsWorld::characterById(u32 id) {
  const auto it = characters_.find(id);
  return it == characters_.end() ? nullptr : &it->second;
}

const CharacterBody* PhysicsWorld::characterById(u32 id) const {
  const auto it = characters_.find(id);
  return it == characters_.end() ? nullptr : &it->second;
}

std::vector<u32> PhysicsWorld::characterIds() const {
  std::vector<u32> ids;
  ids.reserve(characters_.size());
  for (const auto& pair : characters_) ids.push_back(pair.first);  // std::map: ascending
  return ids;
}

void PhysicsWorld::resetCharacter(const Vec3& position) { resetCharacter(kPrimaryCharacter, position); }

void PhysicsWorld::resetCharacter(u32 id, const Vec3& position) {
  CharacterBody* character = characterById(id);
  if (character == nullptr) return;
  character->position = position;
  character->velocity = Vec3{0.0, 0.0, 0.0};
  character->onGround = false;
  character->collisionCount = 0U;
}

bool PhysicsWorld::characterJumpSpeed(f64 takeOffSpeed) {
  return characterJumpSpeed(kPrimaryCharacter, takeOffSpeed);
}

bool PhysicsWorld::characterJumpSpeed(u32 id, f64 takeOffSpeed) {
  CharacterBody* character = characterById(id);
  if (character == nullptr || !character->onGround || takeOffSpeed <= 0.0) return false;
  character->velocity.y = takeOffSpeed;
  character->onGround = false;
  return true;
}

bool PhysicsWorld::characterJump(f64 height) { return characterJump(kPrimaryCharacter, height); }

bool PhysicsWorld::characterJump(u32 id, f64 height) {
  CharacterBody* character = characterById(id);
  if (character == nullptr || !character->onGround) return false;
  character->velocity.y = std::sqrt(2.0 * kGravity * height);
  character->onGround = false;
  return true;
}

void PhysicsWorld::moveCharacter(f64 dt, const Vec3& desiredVelocity) {
  moveCharacter(kPrimaryCharacter, dt, desiredVelocity);
}

void PhysicsWorld::characterCollideAndSlide(CharacterBody& character, u32 selfId, f64 dt) const {
  // One axis at a time, so the character slides along a wall instead of
  // sticking to a corner.
  const f64 beforeX = character.position.x;
  character.position.x += character.velocity.x * dt;
  for (const auto& pair : boxes_) {
    characterResolveAxis(character, pair.second.center, pair.second.halfExtents, 0, beforeX);
  }
  for (const auto& pair : dynamicBoxes_) {
    characterResolveAxis(character, pair.second.position, pair.second.halfExtents, 0, beforeX);
  }
  for (const auto& pair : characters_) {
    if (pair.first == selfId) continue;  // never collide with yourself
    characterResolveAxis(character, pair.second.position, pair.second.halfExtents, 0, beforeX);
  }

  const f64 beforeZ = character.position.z;
  character.position.z += character.velocity.z * dt;
  for (const auto& pair : boxes_) {
    characterResolveAxis(character, pair.second.center, pair.second.halfExtents, 2, beforeZ);
  }
  for (const auto& pair : dynamicBoxes_) {
    characterResolveAxis(character, pair.second.position, pair.second.halfExtents, 2, beforeZ);
  }
  for (const auto& pair : characters_) {
    if (pair.first == selfId) continue;
    characterResolveAxis(character, pair.second.position, pair.second.halfExtents, 2, beforeZ);
  }
}

void PhysicsWorld::characterResolveVertical(CharacterBody& character, u32 selfId, f64 dt) const {
  // Land on top faces while falling, bump the head while rising.
  const bool falling = character.velocity.y <= 0.0;
  const f64 beforeY = character.position.y;
  character.position.y += character.velocity.y * dt;
  if (falling) {
    for (const auto& pair : planes_) {
      if (character.position.y - character.halfExtents.y < pair.second.y) {
        character.position.y = pair.second.y + character.halfExtents.y;
        character.velocity.y = 0.0;
        character.onGround = true;
        ++character.collisionCount;
      }
    }
  }
  for (const auto& pair : boxes_) {
    characterResolveAxis(character, pair.second.center, pair.second.halfExtents, 1, beforeY);
  }
  for (const auto& pair : dynamicBoxes_) {
    characterResolveAxis(character, pair.second.position, pair.second.halfExtents, 1, beforeY);
  }
  for (const auto& pair : characters_) {
    if (pair.first == selfId) continue;
    characterResolveAxis(character, pair.second.position, pair.second.halfExtents, 1, beforeY);
  }
  if (character.velocity.y == 0.0 && falling) character.onGround = true;
}

bool PhysicsWorld::characterBlocked(const CharacterBody& character, u32 selfId,
                                   const Vec3& position) const {
  for (const auto& pair : planes_) {
    if (position.y - character.halfExtents.y < pair.second.y) return true;
  }
  const auto blockedByBox = [&character, &position](const Vec3& center, const Vec3& half) {
    return characterBoxOverlaps(position, character.halfExtents, center, half);
  };
  for (const auto& pair : boxes_) {
    if (blockedByBox(pair.second.center, pair.second.halfExtents)) return true;
  }
  for (const auto& pair : dynamicBoxes_) {
    if (blockedByBox(pair.second.position, pair.second.halfExtents)) return true;
  }
  for (const auto& pair : characters_) {
    if (pair.first == selfId) continue;
    if (blockedByBox(pair.second.position, pair.second.halfExtents)) return true;
  }
  return false;
}

bool PhysicsWorld::characterGroundBelow(const CharacterBody& character, u32 selfId,
                                       const Vec3& position, f64 maxDrop, f64& outFeet) const {
  constexpr f64 kTolerance = 1e-6;
  const f64 feet = position.y - character.halfExtents.y;
  bool found = false;
  f64 best = 0.0;
  const auto consider = [&](f64 top, f64 centerX, f64 centerZ, f64 halfX, f64 halfZ) {
    if (top > feet + kTolerance) return;     // that surface is above the feet
    if (top < feet - maxDrop) return;        // further down than we may drop
    // Under the foot print: see kCharacterGroundOverlap.
    const f64 reachX = halfX + character.halfExtents.x - kCharacterGroundOverlap;
    const f64 reachZ = halfZ + character.halfExtents.z - kCharacterGroundOverlap;
    if (std::abs(position.x - centerX) > reachX) return;
    if (std::abs(position.z - centerZ) > reachZ) return;
    if (found && top <= best) return;
    best = top;
    found = true;
  };
  for (const auto& pair : planes_) consider(pair.second.y, position.x, position.z, 1.0, 1.0);
  for (const auto& pair : boxes_) {
    consider(pair.second.center.y + pair.second.halfExtents.y, pair.second.center.x, pair.second.center.z,
             pair.second.halfExtents.x, pair.second.halfExtents.z);
  }
  for (const auto& pair : dynamicBoxes_) {
    consider(pair.second.position.y + pair.second.halfExtents.y, pair.second.position.x,
             pair.second.position.z, pair.second.halfExtents.x, pair.second.halfExtents.z);
  }
  for (const auto& pair : characters_) {
    if (pair.first == selfId) continue;
    consider(pair.second.position.y + pair.second.halfExtents.y, pair.second.position.x,
             pair.second.position.z, pair.second.halfExtents.x, pair.second.halfExtents.z);
  }
  outFeet = best;
  return found;
}

bool PhysicsWorld::characterStepUp(CharacterBody& character, u32 selfId, f64 dt,
                                  const Vec3& desiredVelocity, const Vec3& startPosition) const {
  CharacterBody trial = character;
  trial.position = startPosition;
  trial.position.y += character.stepHeight;  // phase one: straight up
  if (characterBlocked(trial, selfId, trial.position)) return false;  // no headroom
  trial.velocity.x = desiredVelocity.x;  // the wall may have eaten these
  trial.velocity.z = desiredVelocity.z;
  trial.velocity.y = 0.0;
  characterCollideAndSlide(trial, selfId, dt);  // phase two: across
  f64 top = 0.0;
  // Phase three: drop back onto whatever is under the lifted feet. The drop is
  // the step height plus a hair, so a surface the character could not have
  // reached is not a step.
  if (!characterGroundBelow(trial, selfId, trial.position, character.stepHeight + 1e-3, top)) return false;
  trial.position.y = top + trial.halfExtents.y;
  trial.velocity.y = 0.0;
  trial.onGround = true;
  const auto progress = [&startPosition](const Vec3& position) {
    const f64 dx = position.x - startPosition.x;
    const f64 dz = position.z - startPosition.z;
    return dx * dx + dz * dz;
  };
  if (progress(trial.position) <= progress(character.position) + 1e-9) return false;  // no gain, no step
  // The step still touched something on the way; keep counting it so callers
  // that watch collisionCount see the kerb they walked into.
  trial.collisionCount = character.collisionCount;
  character = trial;
  return true;
}

void PhysicsWorld::moveCharacter(u32 id, f64 dt, const Vec3& desiredVelocity) {
  CharacterBody* self = characterById(id);
  if (self == nullptr) return;
  CharacterBody& character = *self;
  character.collisionCount = 0U;

  // Horizontal control is direct; gravity owns the vertical.
  character.velocity.x = desiredVelocity.x;
  character.velocity.z = desiredVelocity.z;
  if (!character.onGround) {
    character.velocity.y = std::max(character.velocity.y - kGravity * dt, -kMaxCharacterFallSpeed);
  }

  const bool wasGrounded = character.onGround;
  const Vec3 startPosition = character.position;
  characterCollideAndSlide(character, id, dt);
  characterResolveVertical(character, id, dt);
  // Stepping (phase 4): something stopped the ground move short — try to walk up
  // over it instead of into it.
  if (wasGrounded && character.stepHeight > 0.0 && character.collisionCount > 0U) {
    characterStepUp(character, id, dt, desiredVelocity, startPosition);
  }
  // Standing still: verify the support is still there (walking off an edge
  // must drop the character).
  if (character.onGround && !characterSupported(character, id)) character.onGround = false;
}

// --- Raycasting (stage 30) ---

namespace {

// Slab test: where does a ray enter and leave an axis-aligned box?
// Returns false when it misses or the box is entirely behind the origin.
bool rayBox(const Vec3& origin, const Vec3& direction, const Vec3& center, const Vec3& halfExtents,
            f64 maxDistance, f64& outDistance, Vec3& outNormal) {
  f64 nearest = 0.0;
  f64 farthest = maxDistance;
  i32 nearAxis = 0;
  f64 nearSign = 0.0;

  const f64 o[3] = {origin.x, origin.y, origin.z};
  const f64 d[3] = {direction.x, direction.y, direction.z};
  const f64 c[3] = {center.x, center.y, center.z};
  const f64 h[3] = {halfExtents.x, halfExtents.y, halfExtents.z};

  for (i32 axis = 0; axis < 3; ++axis) {
    const f64 lo = c[axis] - h[axis];
    const f64 hi = c[axis] + h[axis];
    if (std::abs(d[axis]) < 1e-12) {
      // Parallel to this pair of faces: a miss unless we start between them.
      if (o[axis] < lo || o[axis] > hi) return false;
      continue;
    }
    const f64 inverse = 1.0 / d[axis];
    f64 t1 = (lo - o[axis]) * inverse;
    f64 t2 = (hi - o[axis]) * inverse;
    f64 sign = -1.0;
    if (t1 > t2) {
      const f64 swap = t1;
      t1 = t2;
      t2 = swap;
      sign = 1.0;
    }
    if (t1 > nearest) {
      nearest = t1;
      nearAxis = axis;
      nearSign = sign;
    }
    if (t2 < farthest) farthest = t2;
    if (nearest > farthest) return false;
  }
  outDistance = nearest;
  outNormal = Vec3{0.0, 0.0, 0.0};
  if (nearAxis == 0) outNormal.x = nearSign;
  else if (nearAxis == 1) outNormal.y = nearSign;
  else outNormal.z = nearSign;
  return true;
}

}  // namespace

PhysicsWorld::RayHit PhysicsWorld::raycast(const Vec3& origin, const Vec3& direction, f64 maxDistance,
                                           u32 ignoreCharacter) const {
  RayHit best;
  const f64 length = direction.length();
  if (length < 1e-12 || maxDistance <= 0.0) return best;
  const Vec3 ray = direction * (1.0 / length);

  const auto consider = [&best](f64 distance, const Vec3& point, const Vec3& normal, u32 character, u32 box,
                                bool ground) {
    if (best.hit && distance >= best.distance) return;
    best.hit = true;
    best.distance = distance;
    best.point = point;
    best.normal = normal;
    best.character = character;
    best.box = box;
    best.ground = ground;
  };

  // --- Static boxes: the walls and cover ---
  for (const auto& entry : boxes_) {
    f64 distance = 0.0;
    Vec3 normal{0.0, 0.0, 0.0};
    if (!rayBox(origin, ray, entry.second.center, entry.second.halfExtents, maxDistance, distance, normal)) {
      continue;
    }
    consider(distance, origin + ray * distance, normal, 0U, entry.first, false);
  }

  // --- Characters: the targets ---
  // A character is its axis-aligned box proxy, so the same slab test does.
  for (const auto& entry : characters_) {
    if (entry.first == ignoreCharacter) continue;  // never shoot yourself
    f64 distance = 0.0;
    Vec3 normal{0.0, 0.0, 0.0};
    if (!rayBox(origin, ray, entry.second.position, entry.second.halfExtents, maxDistance, distance, normal)) {
      continue;
    }
    consider(distance, origin + ray * distance, normal, entry.first, 0U, false);
  }

  // --- Ground planes ---
  if (std::abs(ray.y) > 1e-12) {
    for (const auto& entry : planes_) {
      const f64 distance = (entry.second.y - origin.y) / ray.y;
      if (distance < 0.0 || distance > maxDistance) continue;
      consider(distance, origin + ray * distance, Vec3{0.0, 1.0, 0.0}, 0U, 0U, true);
    }
  }
  return best;
}


}  // namespace kimia
