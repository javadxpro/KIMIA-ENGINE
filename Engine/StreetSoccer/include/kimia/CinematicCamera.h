#pragma once
// =============================================================================
//  CinematicCamera — story-driven virtual camera for trailers.
//
//  Required by phase 1: "دوربین سینمایی مخصوص تریلرها".
//
//  State machine that picks the most cinematic shot based on the
//  current MatchState. The trailer plugin reads the resulting camera
//  every frame and renders via the renderer.
//
//  Shots:
//    EstablishingShot:  3-4 seconds, high + wide, both teams + pitch.
//    HeroShot:          close on the focal player (best trait).
//    ActionShot:        tracks the ball with low FOV during a dribble.
//    GoalMoment:        quick zoom + dolly on scorer.
//    SlowMoCloseUp:     200ms slow-mo when a trick fires.
//    CrowdPullBack:     wide for the final whistle.
//
//  The camera exposes current LookAt + Up + Fov, which any renderer can
//  pick up. It also exposes time-based blend (no jitter) so the render
//  side never sees an instant teleport.
// =============================================================================

#include <kimia/Types.h>
#include <kimia/Vec.h>
#include <string>

namespace kimia::street {

enum class CinematicShot : u8 {
  EstablishingShot,
  HeroShot,
  ActionShot,
  GoalMoment,
  TrickReaction,
  CrowdPullBack,
};

struct CameraView {
  Vec3  eye;
  Vec3  lookAt;
  Vec3  up;
  f32   fov;       // degrees, horizontal
  f32   roll;      // roll in degrees; small for handheld
  f32   zoom;      // dolly multiplier
};

class CinematicCamera {
public:
  // Pass in the most recent match summary and a focal-player index. We
  // pick a shot from those signals.
  void update(float matchTime,                // seconds since kickoff
              int homeScore, int awayScore,    // current scoreboard
              bool ballInAttackZone,          // ball x ≥ pitch.length/2 - 3
              int focalPlayerShirt,            // for hero shots
              float ballSpeed,
              float trickChance);              // 0..1 (×5 boost if a trick fired)

  CinematicShot activeShot() const { return active_; }
  const CameraView& currentView() const { return view_; }
  const char* shotName() const;

private:
  CinematicShot active_ = CinematicShot::EstablishingShot;
  CameraView   view_;
  f32          shotStartTime_ = 0;
  f32          shotDuration_  = 3.0f;
};

}  // namespace kimia::street
