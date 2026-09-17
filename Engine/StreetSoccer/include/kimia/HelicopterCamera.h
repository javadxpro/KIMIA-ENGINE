#pragma once
// =============================================================================
//  HelicopterCamera — GTA-style chopper cam for the Iran Street Soccer trailer.
//
//  Built on top of the basic CinematicCamera. Adds:
//    * Free orbit (mouse / arrow keys)
//    * Smooth "chase" mode that follows the ball
//    * Replayable: ability to scrub time backwards, freeze frame,
//      speed-up to 0.25x / 1x / 4x
//    * Helicopter-stylee: pulsing drift while hovering + slight banking
//      when accelerating.
//
//  Web-friendly:
//    * Outputs a self-describing state each frame (eye / lookAt / fov) — the
//      WebViewer reads this via WebSocket and renders via three.js.
// =============================================================================

#include <kimia/CinematicCamera.h>
#include <kimia/Vec.h>
#include <kimia/Types.h>

namespace kimia::street {

enum class CamMode : u8 {
  Free,      // user-controlled orbit
  Chase,     // follow ball
  Helicopter, // GTA-style: high, smooth, slow orbit
  Replay,    // cinematic scripted
};

struct HelicopterState {
  Vec3  eye;
  Vec3  lookAt;
  f32   fov;
  f32   pitchDeg;    // helicopter dip
  f32   rollDeg;     // bank on turn
  f32   orbitRadius;
  f32   orbitAngle;  // radians around target
  CamMode mode = CamMode::Free;

  // Replay-friendly
  f32   timeScale    = 1.0f;
  bool  freeze       = false;
};

class HelicopterCamera {
public:
  HelicopterCamera();

  // Update the camera. dt = seconds since last update.
  // ballPos follows when chase or helicopter mode active.
  void update(float dt, Vec3 ballPos);

  // User input hooks (called from --interactive flag).
  void nudgeOrbit(float dyaw, float dpitch);
  void zoom(float factor);
  void cycleMode();

  // Replay controls.
  void setTimeScale(float s);
  void toggleFreeze();

  const HelicopterState& state() const { return state_; }

  // Snapshots: pack into a JSON-ish string for the WebViewer to consume
  // via a simple socket. We avoid a JSON dependency in the headless build.
  std::string snapshotJson() const;

private:
  HelicopterState state_;
  CinematicCamera baseCinematic_;
  Vec3 lastBall_ = Vec3(0, 0, 0);

  // Helicopter drift generator (sine-noise breathing).
  float driftPhase_ = 0.0f;
  Vec3  orbitTarget_ = Vec3(0, 0, 0);
};

}  // namespace kimia::street
