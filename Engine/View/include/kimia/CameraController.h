#pragma once

#include <kimia/OrbitCamera.h>
#include <kimia/Picking.h>
#include <kimia/Renderer.h>
#include <kimia/Types.h>
#include <kimia/Vec.h>

namespace kimia {

class WorldEditor;

// What the input layer decides about the camera in one frame. Keeping it a
// plain struct is what makes the camera testable without a window: a test
// feeds the same numbers the keys and the touch pad produce.
struct CameraInput {
  f64 yawDelta = 0.0;    // radians to orbit this frame
  f64 pitchDelta = 0.0;  // radians to raise/lower
  // Multiplicative zoom steps (1.0 = unchanged), applied IN ORDER: zooming
  // twice in one frame is not the same as zooming once by the product when a
  // clamp sits between them, and "q/e/wheel pressed together" is a real case.
  f64 zoomSteps[3] = {1.0, 1.0, 1.0};
  i32 zoomStepCount = 0;
  void zoom(f64 factor) {
    if (factor == 1.0 || zoomStepCount >= 3) return;
    zoomSteps[zoomStepCount++] = factor;
  }
  bool reset = false;    // put the rig back at the default overview
  // True while an editor screen owns the arrows: the distance the user set by
  // hand then becomes the resting distance the director works from. During
  // play the hand-set zoom is remembered instead, so a chase camera pulling
  // back does not destroy it.
  bool handSet = false;
};

// The frame's camera: an orbit rig the user drives, plus the engine's camera
// director (WorldEditor::cameraTarget / cameraDistance / cameraFollowsAim)
// easing it toward where the game wants to look.
//
// This used to live inline in the frame loop, where only a screenshot could
// tell whether the follow easing, the broadcast pull-back or the manual zoom
// still worked. Here it is a pure function of (input, world state, dt), so a
// test can assert on it.
class CameraController {
public:
  CameraController() = default;

  // Applies one frame of orbit/zoom input.
  void applyInput(const CameraInput& input);

  // Eases toward what this world's camera style asks for. Call once per frame
  // with the same dt the simulation used.
  void update(const WorldEditor& editor, f64 dt);

  // Writes view/projection/camera into the frame's scene. The engine decides
  // where to look and how far back to stand; this only turns that decision
  // into a matrix pair.
  void applyTo(RenderScene& scene, i32 width, i32 height) const;

  // The same camera as a pick ray, for "what did the player just tap on".
  pick::Viewport viewport(i32 width, i32 height) const;

  const OrbitCamera& rig() const { return orbit_; }
  Vec3 eye() const { return orbit_.eye(); }
  Vec3 target() const { return orbit_.target(); }
  f64 distance() const { return orbit_.distance; }
  f64 restingDistance() const { return restingDistance_; }
  // The yaw the rig has eased to (radians). Tests read it; the HUD could.
  f64 yaw() const { return orbit_.yaw; }

  // The editor's own starting point (arrow keys orbit, q/e zoom, c resets).
  void reset();

private:
  OrbitCamera orbit_;
  // The distance the user chose by hand: a broadcast camera moves
  // `distance` around every frame, so the manual zoom is kept here and used
  // as the resting point to work from.
  f64 restingDistance_ = OrbitCamera::kDefaultDistance;
};

}  // namespace kimia
