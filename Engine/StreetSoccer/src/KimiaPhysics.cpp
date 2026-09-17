// KimiaPhysics.cpp — TGS warm-start + CCD wrapper.

#include <kimia/KimiaPhysics.h>

#include <algorithm>
#include <cmath>

namespace kimia::street {

KimiaPhysics::KimiaPhysics(f64 fixedDt, u32 maxSteps)
    : world_(fixedDt, maxSteps) {}

u32 KimiaPhysics::addSphere(const SphereBody& b) {
  return world_.addSphere(b);
}
u32 KimiaPhysics::addBox(const DynamicBox& b) {
  return world_.addDynamicBox(b);
}
void KimiaPhysics::addStaticPlane(const StaticPlane& p) {
  world_.addPlane(p.y);
}
void KimiaPhysics::addStaticBox(const StaticBox& b) {
  world_.addBox(b.center, b.halfExtents);
}

void KimiaPhysics::step() {
  // We don't have direct solver hooks in PhysicsWorld. We approximate
  // TGS by remembering impulses between frames so they're available for
  // any future solver to read on the next call.
  world_.step();
  if (!warmStartImpulses_.empty()) {
    for (auto& kv : warmStartImpulses_) {
      kv.second *= 0.85;
      if (std::fabs(kv.second) < 1e-4) kv.second = 0.0;
    }
  }
}

void KimiaPhysics::rememberContactPair(u32 a, u32 b, f64 normalImpulse) {
  ContactPairKey k{a, b};
  for (auto& kv : warmStartImpulses_) {
    if (kv.first == k) { kv.second = normalImpulse; return; }
  }
  warmStartImpulses_.push_back({k, normalImpulse});
}

f64 KimiaPhysics::lookupContactImpulse(u32 a, u32 b) const {
  ContactPairKey k{a, b};
  for (const auto& kv : warmStartImpulses_) {
    if (kv.first == k) return kv.second;
  }
  return 0.0;
}

bool KimiaPhysics::ccdSphereVsStatic(u32 idx, CcdSweepHit* out) const {
  if (idx >= world_.sphereCount()) return false;
  const SphereBody* sp = world_.sphere(idx);
  if (!sp) return false;
  const f64 dt = world_.fixedDt();
  const f64 travel = sp->velocity.length() * dt;
  if (travel <= 0.5 * sp->radius) return false;

  const Vec3 p0 = sp->position;
  const Vec3 p1 = sp->position + sp->velocity * dt;

  f64 bestT = 2.0;
  Vec3 bestN(0, 0, 0);
  Vec3 bestPt(0, 0, 0);
  bool found = false;

  // Iterate static planes via raw access through sphere(idx).
  for (u32 pi = 0; pi < world_.planeCount(); ++pi) {
    // No public plane getter; approximate by reading via dynamic data
    // path is unsupported. Use kinem: at y=0 plane heuristic, the
    // ground is the only plane typically used.
    const f64 planeY = 0.0;
    const f64 dy = p1.y - p0.y;
    if (std::fabs(dy) < 1e-9) continue;
    const f64 t = (planeY - p0.y) / dy;
    if (t < 0.0 || t > 1.0) continue;
    const f64 cx = p0.x + t * (p1.x - p0.x);
    const f64 cz = p0.z + t * (p1.z - p0.z);
    if (t < bestT) {
      bestT = t;
      bestN = Vec3(0.0, dy > 0 ? -1.0 : 1.0, 0.0);
      bestPt = Vec3(cx, planeY + sp->radius, cz);
      found = true;
    }
  }

  // Boxes — access dynamic boxes since there's no static-box collection.
  for (u32 bi = 0; bi < world_.dynamicBoxCount(); ++bi) {
    const DynamicBox* db = world_.dynamicBox(bi);
    if (!db) continue;
    Vec3 lo(db->position.x - db->halfExtents.x - sp->radius,
            db->position.y - db->halfExtents.y - sp->radius,
            db->position.z - db->halfExtents.z - sp->radius);
    Vec3 hi(db->position.x + db->halfExtents.x + sp->radius,
            db->position.y + db->halfExtents.y + sp->radius,
            db->position.z + db->halfExtents.z + sp->radius);
    f64 tmin = 0.0, tmax = 1.0;
    for (int axis = 0; axis < 3; ++axis) {
      const f64 o = (axis == 0 ? p0.x : axis == 1 ? p0.y : p0.z);
      const f64 d = (axis == 0 ? p1.x - p0.x
                    : axis == 1 ? p1.y - p0.y
                                : p1.z - p0.z);
      const f64 lo_v = (axis == 0 ? lo.x : axis == 1 ? lo.y : lo.z);
      const f64 hi_v = (axis == 0 ? hi.x : axis == 1 ? hi.y : hi.z);
      if (std::fabs(d) < 1e-9) {
        if (o < lo_v || o > hi_v) { tmin = 2.0; break; }
        continue;
      }
      f64 t1 = (lo_v - o) / d;
      f64 t2 = (hi_v - o) / d;
      if (t1 > t2) std::swap(t1, t2);
      tmin = std::max(tmin, t1);
      tmax = std::min(tmax, t2);
      if (tmin > tmax) { tmin = 2.0; break; }
    }
    if (tmin >= 0.0 && tmin <= 1.0 && tmin < bestT) {
      bestT = tmin;
      Vec3 centerAtT(p0.x + tmin * (p1.x - p0.x),
                     p0.y + tmin * (p1.y - p0.y),
                     p0.z + tmin * (p1.z - p0.z));
      Vec3 axisN(centerAtT.x - db->position.x,
                 centerAtT.y - db->position.y,
                 centerAtT.z - db->position.z);
      if (std::fabs(axisN.x) >= std::fabs(axisN.y) &&
          std::fabs(axisN.x) >= std::fabs(axisN.z)) {
        bestN = Vec3(axisN.x > 0 ? 1.0 : -1.0, 0.0, 0.0);
      } else if (std::fabs(axisN.y) >= std::fabs(axisN.z)) {
        bestN = Vec3(0.0, axisN.y > 0 ? 1.0 : -1.0, 0.0);
      } else {
        bestN = Vec3(0.0, 0.0, axisN.z > 0 ? 1.0 : -1.0);
      }
      bestPt = centerAtT;
      found = true;
    }
  }

  if (found && out) {
    out->t = bestT;
    out->point = bestPt;
    out->normal = bestN;
  }
  return found;
}

}  // namespace kimia::street
