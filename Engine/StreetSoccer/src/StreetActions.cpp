// StreetActions.cpp — phase-2 move vocabulary for trailers.
//
// All impulse magnitudes are in m/s applied per frame (because the
// underlying PhysicsWorld uses semi-implicit Euler at dt = 1/60s, an
// impulse of 8 m/s translates into 8 m/s velocity per kick, which
// travels 8×(1/60)=13cm before the next step).

#include <kimia/StreetActions.h>

#include <cstdint>

namespace kimia::street {

namespace {
std::vector<StreetAction> g_lib;

ActionStep at(f32 t, f32 ix, f32 iy, f32 iz, bool toBall,
              u8 player = 0, f32 sprint = 1.0f,
              bool goal = false) {
  ActionStep s;
  s.at = t;
  s.impulseX = ix; s.impulseY = iy; s.impulseZ = iz;
  s.toBall = toBall; s.playerIndex = player;
  s.sprintMul = sprint; s.triggersGoal = goal;
  return s;
}

void build_library() {
  if (!g_lib.empty()) return;
  // 0: BackheelTap — backheel pop then chase + first-time volley.
  {
    StreetAction a;
    a.kind = StreetActionKind::BackheelTap;
    a.labelFa = "پشت پا تک ضرب";
    a.labelEn = "BACKHEEL FIRST-TIME";
    a.durationSeconds = 1.4f;
    a.successThreshold = 0.40f;
    a.steps = {
      at(0.00f, 0.0f, 0.0f, 0.0f,  true, 0, 1.0f, false),
      at(0.18f, -1.2f, 0.05f, -0.4f, true, 0, 1.0f, false),  // backheel pop
      at(0.34f, 8.0f,  0.20f, 0.0f, true, 0, 1.0f, false),  // ball flies forward
      at(0.60f, 0.0f, 0.0f, 0.0f,  false, 1, 1.3f, false),
      at(1.10f, 6.0f, 0.40f, 0.0f, true, 1, 1.0f, false),
      at(1.30f, 0.0f, 0.0f, 0.0f, true, 1, 1.0f, true),
    };
    g_lib.push_back(a);
  }
  // 1: FirstTimeVolley — clean one-timer off a dropping cross.
  {
    StreetAction a;
    a.kind = StreetActionKind::FirstTimeVolley;
    a.labelFa = "تک ضرب والی";
    a.labelEn = "FIRST-TIME VOLLEY";
    a.durationSeconds = 0.9f;
    a.successThreshold = 0.50f;
    a.steps = {
      at(0.00f, 0.0f, 0.0f, 0.0f, true, 0, 1.0f, false),
      at(0.05f, 0.0f, -0.2f, 0.0f, true, 0, 1.0f, false),
      at(0.10f, 14.0f, 1.8f, 0.2f, true, 0, 1.0f, false),
      at(0.85f, 0.0f, 0.0f, 0.0f, true, 0, 1.0f, true),
    };
    g_lib.push_back(a);
  }
  // 2: BicycleHeader — overhead kick into goal.
  {
    StreetAction a;
    a.kind = StreetActionKind::BicycleHeader;
    a.labelFa = "دوچرخه هوایی";
    a.labelEn = "BICYCLE HEADER";
    a.durationSeconds = 1.6f;
    a.successThreshold = 0.70f;
    a.steps = {
      at(0.00f, 0.0f, 0.0f, 0.0f, true, 0, 1.0f, false),
      at(0.20f, 4.0f, 5.0f, 1.0f, true, 0, 1.0f, false),
      at(0.85f, 9.0f, -0.5f, 3.0f, true, 0, 1.0f, false),
      at(1.40f, 0.0f, 0.0f, 0.0f, true, 0, 1.0f, true),
    };
    g_lib.push_back(a);
  }
  // 3: GroundCross — sharp pass across the box.
  {
    StreetAction a;
    a.kind = StreetActionKind::GroundCross;
    a.labelFa = "سانر زمینی";
    a.labelEn = "GROUND CROSS";
    a.durationSeconds = 1.0f;
    a.successThreshold = 0.30f;
    a.steps = {
      at(0.00f, 0.0f, 0.0f, 0.0f, true, 0, 1.0f, false),
      at(0.10f, 12.0f, 0.05f, 4.0f, true, 0, 1.0f, false),
      at(0.55f, 0.0f, 0.0f, 0.0f, false, 2, 1.4f, false),
      at(0.85f, 5.5f, 0.20f, 0.0f, true, 2, 1.0f, false),
      at(0.95f, 0.0f, 0.0f, 0.0f, true, 2, 1.0f, true),
    };
    g_lib.push_back(a);
  }
  // 4: ChipPass — lofted ball over defenders.
  {
    StreetAction a;
    a.kind = StreetActionKind::ChipPass;
    a.labelFa = "پاس چیپ";
    a.labelEn = "CHIP PASS";
    a.durationSeconds = 1.1f;
    a.successThreshold = 0.45f;
    a.steps = {
      at(0.00f, 0.0f, 0.0f, 0.0f, true, 0, 1.0f, false),
      at(0.10f, 6.0f, 4.5f, 0.5f, true, 0, 1.0f, false),
      at(0.80f, 0.0f, 0.0f, 0.0f, false, 2, 1.5f, false),
      at(1.05f, 5.0f, 1.2f, 0.0f, true, 2, 1.0f, false),
    };
    g_lib.push_back(a);
  }
  // 5: KeeperWallGoal — kicker's shot redirects off back wall, in.
  {
    StreetAction a;
    a.kind = StreetActionKind::KeeperWallGoal;
    a.labelFa = "شوت دروازه‌بان به دیوار";
    a.labelEn = "KEEPER WALL GOAL";
    a.durationSeconds = 2.4f;
    a.successThreshold = 0.55f;
    a.steps = {
      at(0.00f, 0.0f, 0.0f, 0.0f, true, 0, 1.0f, false),
      at(0.20f, 16.0f, 0.5f, 1.8f, true, 0, 1.0f, false),
      at(0.80f, 0.0f, 0.0f, 0.0f, true, 0, 1.0f, false),
      at(0.95f, -10.0f, 0.4f, 4.0f, true, 0, 1.0f, false),
      at(1.40f, 0.0f, 0.0f, 0.0f, false, 4, 1.5f, false),
      at(1.60f, 6.0f, 0.15f, 0.0f, true, 4, 1.0f, false),
      at(2.20f, 0.0f, 0.0f, 0.0f, true, 4, 1.0f, true),
    };
    g_lib.push_back(a);
  }
  // 6: WheelBreakDribble — fancy 360 past the defender.
  {
    StreetAction a;
    a.kind = StreetActionKind::WheelBreakDribble;
    a.labelFa = "دریبل چرخشی";
    a.labelEn = "WHEEL BREAK DRIBBLE";
    a.durationSeconds = 1.6f;
    a.successThreshold = 0.55f;
    a.steps = {
      at(0.00f, 0.0f, 0.0f, 0.0f, true, 0, 1.0f, false),
      at(0.20f, 2.0f, 0.0f, 0.0f, true, 0, 1.0f, false),
      at(0.60f, -1.5f, 0.0f, 0.0f, true, 0, 1.0f, false),
      at(1.00f, 4.0f, 0.0f, 0.0f, true, 0, 1.5f, false),
      at(1.40f, 5.0f, 0.0f, 0.0f, true, 0, 1.0f, false),
    };
    g_lib.push_back(a);
  }
  // 7: SombreroFlick — ball over defender's head.
  {
    StreetAction a;
    a.kind = StreetActionKind::SombreroFlick;
    a.labelFa = "سومبررو فیک";
    a.labelEn = "SOMBRERO FLICK";
    a.durationSeconds = 1.5f;
    a.successThreshold = 0.60f;
    a.steps = {
      at(0.00f, 0.0f, 0.0f, 0.0f, true, 0, 1.0f, false),
      at(0.10f, 0.0f, 6.0f, 0.0f, true, 0, 1.0f, false),
      at(0.45f, 4.0f, 1.0f, 0.0f, true, 0, 1.0f, false),
      at(0.95f, 0.0f, 0.0f, 0.0f, false, 1, 1.5f, false),
      at(1.30f, 5.5f, 0.0f, 0.0f, true, 1, 1.0f, false),
    };
    g_lib.push_back(a);
  }
  // 8: TallManNutmeg — tall striker receives, nutmeg, runs.
  {
    StreetAction a;
    a.kind = StreetActionKind::TallManNutmeg;
    a.labelFa = "ناتمگ قدبلند";
    a.labelEn = "TALL MAN NUTMEG";
    a.durationSeconds = 1.6f;
    a.successThreshold = 0.50f;
    a.steps = {
      at(0.00f, 0.0f, 0.0f, 0.0f, true, 0, 1.0f, false),
      at(0.20f, 8.0f, 0.0f, 0.0f, true, 0, 1.0f, false),
      at(0.40f, 0.0f, 0.0f, 0.0f, false, 2, 1.0f, false),
      at(0.65f, 4.0f, 0.0f, 0.0f, true, 2, 1.0f, false),
      at(1.10f, 0.0f, 0.0f, 0.0f, false, 2, 1.6f, false),
      at(1.40f, 5.0f, 0.4f, 0.0f, true, 2, 1.0f, false),
    };
    g_lib.push_back(a);
  }
  // 9: FreeKickTopCorner — knuckle over the wall.
  {
    StreetAction a;
    a.kind = StreetActionKind::FreeKickTopCorner;
    a.labelFa = "پنالتی بالای دیوار";
    a.labelEn = "FREE KICK TOP CORNER";
    a.durationSeconds = 1.6f;
    a.successThreshold = 0.65f;
    a.steps = {
      at(0.00f, 0.0f, 0.0f, 0.0f, true, 0, 1.0f, false),
      at(0.15f, 18.0f, 2.5f, -3.0f, true, 0, 1.0f, false),
      at(0.85f, 0.0f, 0.0f, 0.0f, true, 0, 1.0f, false),
      at(1.10f, 0.0f, 0.0f, 0.0f, false, 6, 0.0f, false),
      at(1.40f, 0.0f, 0.0f, 0.0f, true, 0, 1.0f, true),
    };
    g_lib.push_back(a);
  }
}
}  // namespace

const std::vector<StreetAction>& streetActionLibrary() {
  build_library();
  return g_lib;
}

const StreetAction* findAction(StreetActionKind k) {
  build_library();
  for (const auto& a : g_lib) {
    if (a.kind == k) return &a;
  }
  return nullptr;
}

const char* actionLabelFa(StreetActionKind k) {
  auto* p = findAction(k);
  return p ? p->labelFa.c_str() : "?";
}

const char* actionLabelEn(StreetActionKind k) {
  auto* p = findAction(k);
  return p ? p->labelEn.c_str() : "?";
}

std::vector<ActionStep> rollStreetAction(StreetActionKind k,
                                         f32 skillEma,
                                         u32* rngState) {
  const StreetAction* a = findAction(k);
  if (!a) return {};
  if (skillEma + 0.05f < a->successThreshold) return {};
  if (!rngState) return a->steps;
  *rngState = *rngState * 1664525u + 1013904223u;
  if ((*rngState >> 24) % 10 >= 7) return {};
  return a->steps;
}

}  // namespace kimia::street
