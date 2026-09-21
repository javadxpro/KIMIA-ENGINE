#include <kimia/CameraController.h>

#include <kimia/MathUtils.h>
#include <kimia/World.h>

#include <algorithm>
#include <cmath>

namespace kimia {

namespace {

// Shortest signed angle from `from` to `to` (radians).
f64 angleDelta(f64 from, f64 to) {
  f64 delta = std::fmod(to - from + kPi, 2.0 * kPi);
  if (delta < 0.0) delta += 2.0 * kPi;
  return delta - kPi;
}

}  // namespace

void CameraController::applyInput(const CameraInput& input) {
  if (input.reset) orbit_.reset();
  if (input.yawDelta != 0.0 || input.pitchDelta != 0.0) orbit_.orbit(input.yawDelta, input.pitchDelta);
  for (i32 i = 0; i < input.zoomStepCount; ++i) {
    const f64 factor = input.zoomSteps[i];
    if (factor > 0.0) orbit_.zoom(factor);
  }
  // An editor screen owns the arrows, so whatever distance it left behind is
  // the distance the user chose. In play the last hand-set value stands.
  if (input.handSet) restingDistance_ = orbit_.distance;
}

void CameraController::update(const WorldEditor& editor, f64 dt) {
  if (editor.cameraFollowsAim()) {
    // Chase camera: ease around behind the aim. A look drag still peeks
    // around; the camera settles back on its own.
    orbit_.yaw += angleDelta(orbit_.yaw, editor.aimYaw()) * std::min(1.0, kCameraFollowRate * dt);
  }
  orbit_.center = editor.cameraTarget();
  // A broadcast camera pulls back as the play spreads out; ease toward it so
  // the zoom never snaps.
  const f64 wantedDistance = editor.cameraDistance(restingDistance_);
  orbit_.distance += (wantedDistance - orbit_.distance) * std::min(1.0, kCameraFollowRate * dt);
}

void CameraController::applyTo(RenderScene& scene, i32 width, i32 height) const {
  const Vec3 eyeNow = orbit_.eye();
  scene.cameraPosition = eyeNow;
  scene.view = Mat4::lookAt(eyeNow, orbit_.target(), Vec3{0.0, 1.0, 0.0});
  const f64 aspect = height > 0 ? static_cast<f64>(width) / static_cast<f64>(height) : 1.0;
  scene.projection = Mat4::perspective(radians(60.0), aspect, 0.1, 100.0);
  scene.lightDirection = Vec3{-0.4, -0.8, -0.4};
}

pick::Viewport CameraController::viewport(i32 width, i32 height) const {
  pick::Viewport out;
  out.view = Mat4::lookAt(orbit_.eye(), orbit_.target(), Vec3{0.0, 1.0, 0.0});
  const f64 aspect = height > 0 ? static_cast<f64>(width) / static_cast<f64>(height) : 1.0;
  out.projection = Mat4::perspective(radians(60.0), aspect, 0.1, 100.0);
  out.eye = orbit_.eye();
  out.width = width;
  out.height = height;
  return out;
}

void CameraController::reset() {
  orbit_.reset();
  restingDistance_ = orbit_.distance;
}

}  // namespace kimia
