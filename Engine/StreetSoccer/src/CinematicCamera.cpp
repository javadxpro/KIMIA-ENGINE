// CinematicCamera.cpp — picks the right shot from the match state.

#include <kimia/CinematicCamera.h>

#include <cmath>

namespace kimia::street {

static const Vec3 up_default{0, 1, 0};

static CameraView make_establishing() {
  CameraView v;
  v.eye    = Vec3(0, 18.0, 26.0);  // high + behind home goal
  v.lookAt = Vec3(0, 0, 0);
  v.up     = up_default;
  v.fov    = 60.0f;
  v.roll   = 0.0f;
  v.zoom   = 1.0f;
  return v;
}

static CameraView make_hero(int jerseyNumber, bool leftSide) {
  CameraView v;
  const f32 x = leftSide ? -6.0f : 6.0f;
  v.eye    = Vec3(x + static_cast<double>(jerseyNumber % 3) * 1.5, 1.6f, 4.0f);
  v.lookAt = Vec3(x, 1.4, 0.0);
  v.up     = up_default;
  v.fov    = 35.0f;
  v.roll   = (jerseyNumber % 2 == 0) ? 2.5f : -1.8f;  // subtle handheld
  v.zoom   = 1.1f;
  return v;
}

static CameraView make_action(float ballSpeed) {
  CameraView v;
  // Ball-with: camera just behind, low.
  v.eye    = Vec3(0.0, 1.2, -3.0);
  v.lookAt = Vec3(0.0, 0.5, 0.0);
  v.up     = up_default;
  v.fov    = std::fmax(40.0f, 70.0f - ballSpeed * 6.0f);  // tighter for fast dribbles
  v.roll   = 0.0f;
  v.zoom   = 1.0f;
  return v;
}

static CameraView make_goal(int jerseyNumber) {
  CameraView v;
  // Sweep around scorer.
  v.eye    = Vec3((jerseyNumber % 2 ? 3.0f : -3.0f), 1.2f, 4.0f);
  v.lookAt = Vec3(0.0, 1.5, 0.0);
  v.up     = up_default;
  v.fov    = 30.0f;
  v.roll   = -4.0f;
  v.zoom   = 1.4f;
  return v;
}

static CameraView make_trick() {
  CameraView v;
  // Close, slightly low, hand-held.
  v.eye    = Vec3(2.0, 1.0, 2.5);
  v.lookAt = Vec3(0.0, 0.8, 0.0);
  v.up     = up_default;
  v.fov    = 28.0f;
  v.roll   = 6.0f;
  v.zoom   = 1.2f;
  return v;
}

static CameraView make_pullback() {
  CameraView v;
  // Drone shot from above.
  v.eye    = Vec3(0.0, 35.0, -30.0);
  v.lookAt = Vec3(0, 0.0, 0.0);
  v.up     = up_default;
  v.fov    = 75.0f;
  v.roll   = 0.0f;
  v.zoom   = 1.0f;
  return v;
}

const char* CinematicCamera::shotName() const {
  switch (active_) {
    case CinematicShot::EstablishingShot: return "ESTABLISHING";
    case CinematicShot::HeroShot:         return "HERO";
    case CinematicShot::ActionShot:       return "ACTION";
    case CinematicShot::GoalMoment:       return "GOAL REACTION";
    case CinematicShot::TrickReaction:    return "TRICK REACTION";
    case CinematicShot::CrowdPullBack:    return "PULL BACK";
  }
  return "?";
}

void CinematicCamera::update(float matchTime, int homeScore,
                             int /*awayScore*/, bool ballInAttackZone,
                             int focalPlayerShirt, float ballSpeed,
                             float trickChance) {
  // Drive a shot schedule.
  // 0–4 s: Establishing
  // 4–8 s: Hero on home #2
  // 8–28 s: Action
  // 28–35 s: Hero on away #3
  // 35–end: Pull back
  // Trick replaces current shot briefly if a trick just fired (trickChance > 0.6).
  CinematicShot desired;
  if (trickChance > 0.6f) {
    desired = CinematicShot::TrickReaction;
    shotDuration_ = 1.4f;
  } else if (matchTime < 4.0f) {
    desired = CinematicShot::EstablishingShot;
    shotDuration_ = 4.0f;
  } else if (matchTime < 8.0f) {
    desired = CinematicShot::HeroShot;
    shotDuration_ = 4.0f;
  } else if (matchTime < 28.0f) {
    desired = ballInAttackZone ? CinematicShot::ActionShot : CinematicShot::ActionShot;
    shotDuration_ = 2.0f;
  } else if (matchTime < 35.0f) {
    desired = CinematicShot::HeroShot;
    shotDuration_ = 6.0f;
  } else {
    desired = CinematicShot::CrowdPullBack;
    shotDuration_ = 2.0f;
  }

  if (desired != active_ || matchTime > shotStartTime_ + shotDuration_) {
    active_ = desired;
    shotStartTime_ = matchTime;
    // compute new view
    switch (active_) {
      case CinematicShot::EstablishingShot:    view_ = make_establishing(); break;
      case CinematicShot::HeroShot:            view_ = make_hero(focalPlayerShirt, focalPlayerShirt < 5); break;
      case CinematicShot::ActionShot:          view_ = make_action(ballSpeed); break;
      case CinematicShot::GoalMoment:          view_ = make_goal(focalPlayerShirt); break;
      case CinematicShot::TrickReaction:       view_ = make_trick(); break;
      case CinematicShot::CrowdPullBack:       view_ = make_pullback(); break;
    }
  } else {
    // smoothly ease FOV toward target so cuts are less jarring.
    switch (active_) {
      case CinematicShot::ActionShot: {
        const CameraView target = make_action(ballSpeed);
        view_.fov = view_.fov * 0.85f + target.fov * 0.15f;
      } break;
      default: break;
    }
  }
  (void)homeScore;
}

}  // namespace kimia::street
