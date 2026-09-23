// kimia_bench_physics — what the physics step costs, at 10, 100 and 1000 bodies.
//
// Phase 4 asks for numbers, not adjectives: "the broad phase kills O(n^2)" is
// an assertion until someone runs it at 10, 100 and 1000 bodies and prints the
// pair count. This tool builds the two scenes that matter for a street pitch —
// loose balls scattered over the field, and a dense pile of crates — and
// reports, per body count:
//
//   * milliseconds per fixed step (the number a frame budget is spent from),
//   * narrow-phase pair tests per step (the work the broad phase saved),
//   * what an all-pairs scan would have paid (the number to compare against),
//   * how many pair candidates the sweep offered and how many of them needed a test.
//
// It is a benchmark, not a test: nothing here asserts a wall-clock time, so a
// slow CI machine cannot fail the build. The tests (Tests/src/PhysicsTests.cpp)
// pin the PROPERTIES instead — bit-identical results with and without the grid,
// and at least an order of magnitude fewer pair tests at 100 bodies.
//
// Usage: kimia_bench_physics [steps] [--linear] [--no-ccd]
//   steps    fixed steps per scene (default 120 = one second of simulation)
//   --linear run the all-pairs path too (slow: it is the thing being replaced)
//   --no-ccd run the discrete (no sweep) sphere integration, to price the CCD
#include <kimia/Physics.h>
#include <kimia/Types.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using kimia::DynamicBox;
using kimia::PhysicsWorld;
using kimia::SphereBody;
using kimia::Vec3;
using kimia::f64;
using kimia::u32;
using kimia::usize;

// Two deterministic scatter walks. Nothing here is random: a benchmark that
// moves between runs cannot be compared between runs.
constexpr f64 kGolden = 0.6180339887498949;
constexpr f64 kSilver = 0.7548776662466927;

struct Scene {
  std::string name;
  usize spheres = 0U;
  usize boxes = 0U;
  f64 spread = 20.0;   // metres across the field
  f64 stackGap = 0.0;  // > 0: pile the boxes at the origin instead of spreading
};

PhysicsWorld build(const Scene& scene) {
  PhysicsWorld world;
  world.character()->position = Vec3{0.0, 0.5, -39.0};  // the player, out of the way
  world.addPlane(0.0);
  for (usize i = 0; i < scene.spheres; ++i) {
    SphereBody ball;
    const f64 t = static_cast<f64>(i);
    ball.position = Vec3{std::fmod(t * kGolden, 1.0) * scene.spread - scene.spread * 0.5,
                         2.0 + t * 0.01,
                         std::fmod(t * kSilver, 1.0) * scene.spread - scene.spread * 0.5};
    ball.velocity = Vec3{0.4, 0.0, -0.3};
    world.addSphere(ball);
  }
  for (usize i = 0; i < scene.boxes; ++i) {
    DynamicBox box;
    const f64 t = static_cast<f64>(i);
    if (scene.stackGap > 0.0) {
      // A pile: 4 x 4 towers, stacked up until the boxes run out.
      box.position =
          Vec3{static_cast<f64>(i % 4U) * scene.stackGap - 1.5,
               0.5 + static_cast<f64>(i / 16U) * scene.stackGap,
               static_cast<f64>((i / 4U) % 4U) * scene.stackGap - 1.5};
    } else {
      box.position = Vec3{std::fmod(t * 0.3819660112501051, 1.0) * scene.spread - scene.spread * 0.5,
                          0.5 + t * 0.5,
                          std::fmod(t * 0.5698402909980532, 1.0) * scene.spread - scene.spread * 0.5};
    }
    world.addDynamicBox(box);
  }
  return world;
}

struct Result {
  f64 msPerStep = 0.0;
  f64 pairTestsPerStep = 0.0;
  f64 candidatePairsPerStep = 0.0;
  f64 allPairsPerCollect = 0.0;
};

Result measure(PhysicsWorld& world, u32 steps) {
  world.resetStats();
  // Warm-up: the first steps allocate the grid's buckets, and counting that
  // once would make every small body count look slow.
  for (u32 i = 0; i < 10U; ++i) world.step();
  world.resetStats();
  const auto start = std::chrono::steady_clock::now();
  for (u32 i = 0; i < steps; ++i) world.step();
  const auto finish = std::chrono::steady_clock::now();
  const f64 seconds = std::chrono::duration<f64>(finish - start).count();

  Result result;
  const usize bodies = world.sphereCount() + world.dynamicBoxCount();
  result.msPerStep = seconds * 1000.0 / static_cast<f64>(steps);
  result.pairTestsPerStep = static_cast<f64>(world.stats().pairTests) / static_cast<f64>(steps);
  result.candidatePairsPerStep =
      static_cast<f64>(world.stats().candidatePairs) / static_cast<f64>(steps);
  result.allPairsPerCollect =
      static_cast<f64>(bodies) * static_cast<f64>(bodies - 1U) / 2.0;
  return result;
}

void printScene(const Scene& scene, u32 steps, bool linear, bool ccd) {
  std::printf("\n%s  (%zu spheres + %zu crates = %zu dynamic bodies, ccd %s)\n", scene.name.c_str(), scene.spheres,
              scene.boxes, scene.spheres + scene.boxes, ccd ? "on" : "off");
  std::printf("  %-9s %12s %16s %16s %14s\n", "path", "ms/step", "pairs/step", "candidates/step",
              "all-pairs");

  PhysicsWorld grid = build(scene);
  grid.setBroadPhaseEnabled(true);
  grid.setCcdEnabled(ccd);
  const Result withGrid = measure(grid, steps);
  std::printf("  %-9s %12.3f %16.0f %16.0f %14.0f\n", "sweep", withGrid.msPerStep,
              withGrid.pairTestsPerStep, withGrid.candidatePairsPerStep, withGrid.allPairsPerCollect);

  if (linear) {
    PhysicsWorld plain = build(scene);
    plain.setBroadPhaseEnabled(false);
    plain.setCcdEnabled(ccd);
    const Result without = measure(plain, steps);
    std::printf("  %-9s %12.3f %16.0f %16s %14.0f\n", "all-pairs", without.msPerStep,
                without.pairTestsPerStep, "-", without.allPairsPerCollect);
    if (without.msPerStep > 0.0) {
      std::printf("  speedup: %.1fx, pair-work reduction: %.1fx\n",
                  without.msPerStep / (withGrid.msPerStep > 0.0 ? withGrid.msPerStep : 1e-9),
                  without.pairTestsPerStep / (withGrid.pairTestsPerStep > 0.0 ? withGrid.pairTestsPerStep : 1e-9));
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  u32 steps = 120U;  // one second of simulation at the fixed 120 Hz step
  bool linear = false;
  bool ccd = true;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--linear") {
      linear = true;
      continue;
    }
    if (arg == "--no-ccd") {
      ccd = false;
      continue;
    }
    steps = static_cast<u32>(std::max(1, std::atoi(arg.c_str())));
  }

  std::printf("KIMIA physics benchmark — %u fixed steps per scene, %.0f Hz, solver 20 iterations\n",
              steps, 1.0 / PhysicsWorld{}.fixedDt());
  if (!linear) std::printf("(add --linear to also run the all-pairs path being replaced)\n");

  const Scene loose10{"field: 10 loose bodies", 6U, 4U, 20.0, 0.0};
  const Scene loose100{"field: 100 loose bodies", 60U, 40U, 20.0, 0.0};
  const Scene loose1000{"field: 1000 loose bodies", 600U, 400U, 40.0, 0.0};
  const Scene pile100{"pile: 100 crates stacked in the middle", 0U, 100U, 4.0, 1.01};
  const Scene mixed1000{"pitch: 1000 bodies, half of them in a pile", 500U, 500U, 40.0, 1.01};

  printScene(loose10, steps, true, ccd);
  printScene(loose100, steps, true, ccd);
  printScene(loose1000, steps, linear, ccd);
  printScene(pile100, steps, true, ccd);
  printScene(mixed1000, steps, linear, ccd);
  std::printf("\n");
  return 0;
}
