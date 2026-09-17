#pragma once
// =============================================================================
//  Replay System (Brazil Football Street / فوتبال خیابونی ایران)
//
//  Records a stream of snapshots of the MatchState during a match and can
//  play them back at any speed. Each snapshot is a minimal delta (positions
//  + velocities + scores) so memory stays bounded.
//
//  Snapshot storage: ring buffer with a configurable max frame count.
//  Determinism: the seed is recorded with the first frame so replays can be
//  reproduced bit-for-bit.
// =============================================================================

#include <kimia/StreetSoccer.h>
#include <kimia/Types.h>
#include <vector>
#include <string>

namespace kimia::street {

struct ReplayFrame {
  f32 time = 0.0f;
  f32 ballX = 0.0f;
  f32 ballY = 0.0f;
  f32 ballVx = 0.0f;
  f32 ballVy = 0.0f;
  i32 homeScore = 0;
  i32 awayScore = 0;
  // Per-player snapshot (indexed by team + slot).
  struct PlayerSnap {
    u8 shirtNumber = 0;
    f32 x = 0;
    f32 y = 0;
    f32 vx = 0;
    f32 vy = 0;
    bool hasBall = false;
  };
  std::vector<PlayerSnap> homePlayers;
  std::vector<PlayerSnap> awayPlayers;
};

class ReplayRecorder {
 public:
  explicit ReplayRecorder(std::size_t maxFrames = 18000);  // 5 min @ 60fps

  // Start a new recording; clears any existing data.
  void begin(u32 seed);

  // Capture one frame from the current match state.
  void captureFrame(const MatchState& state, f32 time);

  // Stop recording; freeze the buffer.
  void end();

  // Returns true if currently recording.
  bool isRecording() const { return recording_; }

  // Number of frames stored.
  std::size_t frameCount() const { return frames_.size(); }

  // Seed of the first frame (0 if no recording).
  u32 seed() const { return seed_; }

  // Total duration of the recording.
  f32 duration() const;

  // Read-only access to frames for playback.
  const std::vector<ReplayFrame>& frames() const { return frames_; }

  // Frame at fractional time t (linear interpolation between two frames).
  // Returns false if t is out of range.
  bool frameAt(f32 t, ReplayFrame& out) const;

 private:
  std::size_t maxFrames_ = 18000;
  std::vector<ReplayFrame> frames_;
  bool recording_ = false;
  u32 seed_ = 0;
};

class ReplayPlayer {
 public:
  // Begin playback of a recording.
  void begin(const ReplayRecorder& recorder, f32 speed = 1.0f);

  // Stop playback.
  void end();

  // Update playback time (call per host frame).
  void tick(f32 hostDt);

  // Get the current interpolated frame; returns false if no playback.
  bool currentFrame(ReplayFrame& out) const;

  // Is playback active?
  bool isPlaying() const { return playing_; }

  // Playback time in seconds.
  f32 time() const { return time_; }

  // Speed multiplier.
  f32 speed() const { return speed_; }
  void setSpeed(f32 s) { speed_ = s; }

  // Total length of the loaded recording in seconds.
  f32 length() const { return length_; }

 private:
  bool playing_ = false;
  f32 time_ = 0.0f;
  f32 speed_ = 1.0f;
  f32 length_ = 0.0f;
  const ReplayRecorder* rec_ = nullptr;
};

// Serialize a frame to a compact byte buffer (for save-to-disk).
std::vector<u8> serializeFrame(const ReplayFrame& f);

// Load a frame from a byte buffer.
bool deserializeFrame(const u8* data, std::size_t size, ReplayFrame& out);

}  // namespace kimia::street
