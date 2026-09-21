// What the camera looks at, and the distance it sits at.
//
// Split out of World.cpp with no behaviour change. Two families of answers
// live here, and the difference between them is deliberate:
//
//   * cameraTarget()/cameraTargetEntity()/aliveCameraTarget() choose the
//     SUBJECT: the entity a CameraTargetComponent names wins, otherwise the
//     ball while playing and the selected object while editing.
//   * cameraDistance()/cameraFollowsAim() are the shape of the rig the view
//     layer builds from that subject.
//
// The camera rig itself (position, orbit, smoothing) is Engine/View's
// CameraController — this file answers questions, it does not move anything.
#include "WorldInternal.h"

#include <kimia/World.h>

#include <algorithm>
#include <cmath>

namespace kimia {

using namespace worldinternal;  // the World module's own toolbox (see the header)


Vec3 WorldEditor::cameraTarget() const {
  if (placing() || movingObject()) return Vec3{ghost_.x, 0.2, ghost_.z};
  // A world may say what the camera should watch (CameraTargetComponent): the
  // highest weight wins, ties go to the lowest handle, the same rule the name
  // index follows. While the person is working on an object (selection
  // screens) their own hands win — an authored target must not fight the
  // editor.
  if (!selectingObject()) {
    const EntityHandle wanted = cameraTargetEntity();
    if (aliveCameraTarget(wanted)) {
      const EntityData* target = world_.scene.get(wanted);
      return target->transform.position + target->cameraTarget->offset;
    }
  }
  if (!playing()) {
    const EntityData* selected = selectedEntity();
    if (selectingObject() && selected != nullptr) return selected->transform.position;
    return Vec3{0.0, 0.2, 0.0};
  }
  const Vec3 ball = ballPosition();
  if (world_.profile.camera != CameraStyle::Broadcast) return ball;
  // Broadcast: frame the play between the ball and the player, weighted
  // toward the ball because that is what the viewer is actually watching.
  return Vec3{ball.x * kCameraBallBias + playerPos_.x * (1.0 - kCameraBallBias), ball.y,
              ball.z * kCameraBallBias + playerPos_.z * (1.0 - kCameraBallBias)};
}

EntityHandle WorldEditor::cameraTargetEntity() const {
  EntityHandle best = kNullEntity;
  f64 bestWeight = 0.0;
  world_.scene.forEach([&](EntityHandle handle, const EntityData& entity) {
    if (!entity.cameraTarget.has_value()) return;
    // An edit-only target does not steer the camera during play.
    if (playing() && !entity.cameraTarget->whilePlaying) return;
    const f64 weight = entity.cameraTarget->weight;
    if (weight <= bestWeight) return;  // ties keep the lower handle
    best = handle;
    bestWeight = weight;
  });
  return best;
}

bool WorldEditor::aliveCameraTarget(EntityHandle handle) const {
  if (handle == kNullEntity) return false;
  const EntityData* entity = world_.scene.get(handle);
  return entity != nullptr && entity->cameraTarget.has_value();
}

f64 WorldEditor::cameraDistance(f64 restingDistance) const {
  if (!playing() || world_.profile.camera != CameraStyle::Broadcast) return restingDistance;
  // Pull back as the ball and the player separate, so a long ball never
  // leaves half the play off screen.
  const Vec3 ball = ballPosition();
  const f64 dx = ball.x - playerPos_.x;
  const f64 dz = ball.z - playerPos_.z;
  const f64 spread = std::sqrt(dx * dx + dz * dz);
  const f64 wanted = kCameraBroadcastNear + spread * kCameraBroadcastPerMeter;
  return std::min(kCameraBroadcastFar, std::max(kCameraBroadcastNear, wanted));
}

bool WorldEditor::cameraFollowsAim() const {
  if (!playing() || roundOver()) return false;
  // Only a chase camera swings around behind the aim. A broadcast camera
  // holds its side of the pitch, like a real touchline camera: swinging it
  // around behind the player every time they turn would be unwatchable.
  return world_.profile.camera == CameraStyle::Chase;
}

}  // namespace kimia
