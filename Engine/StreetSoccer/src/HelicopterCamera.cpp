// HelicopterCamera.cpp — GTA-chopper feel + replay scrub.

#include <kimia/HelicopterCamera.h>

#include <cmath>
#include <cstdio>

namespace kimia::street {

HelicopterCamera::HelicopterCamera() {
  state_.eye         = Vec3(0.0, 14.0, 18.0);
  state_.lookAt      = Vec3(0.0, 0.0, 0.0);
  state_.fov         = 60.0f;
  state_.orbitRadius = 22.0f;
  state_.orbitAngle  = 0.0f;
  state_.mode        = CamMode::Chase;
  state_.pitchDeg    = 30.0f;
}

void HelicopterCamera::update(float dt, Vec3 ballPos) {
  driftPhase_ += dt * 0.5f;
  orbitTarget_ = ballPos;

  switch (state_.mode) {
    case CamMode::Helicopter: {
      // Slow orbit. dt controls angular speed (rad/sec).
      state_.orbitAngle += dt * 0.20f;

      // Helicopter "breathing" — small sinusoid.
      const f32 breathe = std::sin(driftPhase_ * 2.5f) * 0.6f;
      const f32 dip     = std::sin(driftPhase_ * 1.3f) * 0.4f;

      state_.pitchDeg = 28.0f + dip * 3.0f;
      state_.rollDeg  = std::sin(driftPhase_) * 4.0f;
      state_.fov      = 55.0f + breathe;

      const f32 r = state_.orbitRadius;
      const f32 px = std::cos(state_.orbitAngle);
      const f32 pz = std::sin(state_.orbitAngle);
      const f32 py = std::tan(state_.pitchDeg * 3.14159265f / 180.0f) * r;

      state_.eye = Vec3(orbitTarget_.x + px * r,
                        orbitTarget_.y + py + breathe,
                        orbitTarget_.z + pz * r);
      state_.lookAt = Vec3(orbitTarget_.x,
                           orbitTarget_.y + 0.4f,
                           orbitTarget_.z);
      // Subtle chase bias: when ball moves, lean toward velocity.
      const Vec3 ballVel = ballPos - lastBall_;
      state_.lookAt.x += ballVel.x * 0.1f;
      state_.lookAt.z += ballVel.z * 0.1f;
      lastBall_ = ballPos;
    } break;

    case CamMode::Chase: {
      state_.fov = 50.0f;
      state_.rollDeg = 0.0f;
      state_.pitchDeg = 15.0f;
      const f32 r = 7.0f;
      state_.eye = Vec3(ballPos.x,
                        ballPos.y + 4.0f,
                        ballPos.z - r);
      state_.lookAt = Vec3(ballPos.x + (ballPos.x - lastBall_.x) * 0.3f,
                           ballPos.y,
                           ballPos.z + (ballPos.z - lastBall_.z) * 0.3f);
      lastBall_ = ballPos;
    } break;

    case CamMode::Free: {
      // nothing to do; user controls via nudgeOrbit / zoom.
      state_.rollDeg = 0.0f;
      state_.pitchDeg = 25.0f;
    } break;

    case CamMode::Replay: {
      // Cycle through hero / action / pull back depending on timeScale.
      const float t = driftPhase_;
      if (state_.timeScale < 0.5f) {
        // Slow-mo, hero close.
        state_.fov = 30.0f;
        state_.eye = Vec3(state_.eye.x + std::cos(t) * 4.0f,
                          state_.eye.y + 1.0f,
                          state_.eye.z + std::sin(t) * 4.0f);
        state_.rollDeg = 6.0f;
      } else {
        state_.fov = 60.0f;
        state_.rollDeg = 0.0f;
      }
    } break;
  }
}

void HelicopterCamera::nudgeOrbit(float dyaw, float dpitch) {
  state_.orbitAngle  += dyaw;
  state_.pitchDeg    += dpitch;
  if (state_.pitchDeg > 75.0f) state_.pitchDeg = 75.0f;
  if (state_.pitchDeg < 5.0f)  state_.pitchDeg = 5.0f;
}

void HelicopterCamera::zoom(float factor) {
  state_.orbitRadius *= factor;
  if (state_.orbitRadius < 4.0f)  state_.orbitRadius = 4.0f;
  if (state_.orbitRadius > 60.0f) state_.orbitRadius = 60.0f;
}

void HelicopterCamera::cycleMode() {
  switch (state_.mode) {
    case CamMode::Free:       state_.mode = CamMode::Chase;      break;
    case CamMode::Chase:      state_.mode = CamMode::Helicopter; break;
    case CamMode::Helicopter: state_.mode = CamMode::Replay;     break;
    case CamMode::Replay:     state_.mode = CamMode::Free;       break;
  }
}

void HelicopterCamera::setTimeScale(float s) { state_.timeScale = s; }
void HelicopterCamera::toggleFreeze()        { state_.freeze = !state_.freeze; }

std::string HelicopterCamera::snapshotJson() const {
  char buf[512];
  std::snprintf(buf, sizeof buf,
    "{"
    "\"eye\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f},"
    "\"look\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f},"
    "\"fov\":%.2f,\"roll\":%.2f,\"pitch\":%.2f,"
    "\"radius\":%.2f,\"angle\":%.3f,\"mode\":%u,"
    "\"timeScale\":%.2f,\"freeze\":%s"
    "}",
    state_.eye.x, state_.eye.y, state_.eye.z,
    state_.lookAt.x, state_.lookAt.y, state_.lookAt.z,
    state_.fov, state_.rollDeg, state_.pitchDeg,
    state_.orbitRadius, state_.orbitAngle,
    static_cast<unsigned>(state_.mode),
    state_.timeScale,
    state_.freeze ? "true" : "false");
  return std::string(buf);
}

}  // namespace kimia::street
