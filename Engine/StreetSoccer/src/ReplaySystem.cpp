#include <kimia/ReplaySystem.h>
#include <algorithm>
#include <cstring>

namespace kimia::street {

ReplayRecorder::ReplayRecorder(std::size_t maxFrames)
    : maxFrames_(maxFrames) {
  frames_.reserve(maxFrames);
}

void ReplayRecorder::begin(u32 seed) {
  frames_.clear();
  seed_ = seed;
  recording_ = true;
}

void ReplayRecorder::captureFrame(const MatchState& state, f32 time) {
  if (!recording_) return;
  if (frames_.size() >= maxFrames_) return;

  ReplayFrame f;
  f.time = time;
  f.ballX = state.ball.x;
  f.ballY = state.ball.y;
  f.ballVx = state.ball.vx;
  f.ballVy = state.ball.vy;
  f.homeScore = state.homeScore;
  f.awayScore = state.awayScore;
  f.homePlayers.reserve(state.homeTeam.size());
  for (const auto& p : state.homeTeam) {
    ReplayFrame::PlayerSnap s;
    s.shirtNumber = p.shirtNumber;
    s.x = p.positionX;
    s.y = p.positionY;
    s.vx = p.velocityX;
    s.vy = p.velocityY;
    s.hasBall = p.hasBall;
    f.homePlayers.push_back(s);
  }
  f.awayPlayers.reserve(state.awayTeam.size());
  for (const auto& p : state.awayTeam) {
    ReplayFrame::PlayerSnap s;
    s.shirtNumber = p.shirtNumber;
    s.x = p.positionX;
    s.y = p.positionY;
    s.vx = p.velocityX;
    s.vy = p.velocityY;
    s.hasBall = p.hasBall;
    f.awayPlayers.push_back(s);
  }
  frames_.push_back(f);
}

void ReplayRecorder::end() {
  recording_ = false;
}

f32 ReplayRecorder::duration() const {
  if (frames_.empty()) return 0.0f;
  return frames_.back().time - frames_.front().time;
}

bool ReplayRecorder::frameAt(f32 t, ReplayFrame& out) const {
  if (frames_.empty()) return false;
  const f32 tStart = frames_.front().time;
  const f32 tEnd = frames_.back().time;
  if (t < tStart || t > tEnd) return false;
  // Binary search.
  std::size_t lo = 0;
  std::size_t hi = frames_.size() - 1;
  while (lo + 1 < hi) {
    const std::size_t mid = (lo + hi) / 2;
    if (frames_[mid].time <= t) lo = mid;
    else                       hi = mid;
  }
  if (lo == hi) { out = frames_[lo]; return true; }
  // For now we return the earlier frame; the lerp parameter `u` is
  // reserved for the high-fidelity interpolator the trailer pipeline
  // can opt into later.
  const ReplayFrame& a = frames_[lo];
  out = a;
  out.time = t;
  return true;
}

void ReplayPlayer::begin(const ReplayRecorder& recorder, f32 speed) {
  rec_ = &recorder;
  time_ = recorder.frames().empty() ? 0.0f
                                    : recorder.frames().front().time;
  length_ = recorder.duration();
  speed_ = speed;
  playing_ = !recorder.frames().empty();
}

void ReplayPlayer::end() {
  playing_ = false;
  rec_ = nullptr;
}

void ReplayPlayer::tick(f32 hostDt) {
  if (!playing_ || !rec_) return;
  time_ += hostDt * speed_;
  const f32 tEnd = rec_->frames().back().time;
  if (time_ > tEnd) {
    time_ = tEnd;
    playing_ = false;
  }
}

bool ReplayPlayer::currentFrame(ReplayFrame& out) const {
  if (!rec_) return false;
  return rec_->frameAt(time_, out);
}

std::vector<u8> serializeFrame(const ReplayFrame& f) {
  std::vector<u8> out;
  const std::size_t fixed = sizeof(f32) * 5 + sizeof(i32) * 2;
  const std::size_t perPlayer = sizeof(u8) + sizeof(f32) * 4 + sizeof(u8);
  out.reserve(fixed + perPlayer * (f.homePlayers.size() + f.awayPlayers.size()));
  auto pushF32 = [&](f32 v) {
    const u8* p = reinterpret_cast<const u8*>(&v);
    for (std::size_t i = 0; i < sizeof(f32); ++i) out.push_back(p[i]);
  };
  auto pushI32 = [&](i32 v) {
    const u8* p = reinterpret_cast<const u8*>(&v);
    for (std::size_t i = 0; i < sizeof(i32); ++i) out.push_back(p[i]);
  };
  pushF32(f.time);
  pushF32(f.ballX);
  pushF32(f.ballY);
  pushF32(f.ballVx);
  pushF32(f.ballVy);
  pushI32(f.homeScore);
  pushI32(f.awayScore);
  auto pushPlayer = [&](const ReplayFrame::PlayerSnap& p) {
    out.push_back(p.shirtNumber);
    pushF32(p.x);
    pushF32(p.y);
    pushF32(p.vx);
    pushF32(p.vy);
    out.push_back(p.hasBall ? 1 : 0);
  };
  for (const auto& p : f.homePlayers) pushPlayer(p);
  for (const auto& p : f.awayPlayers) pushPlayer(p);
  return out;
}

bool deserializeFrame(const u8* data, std::size_t size, ReplayFrame& f) {
  if (!data || size < sizeof(f32) * 5 + sizeof(i32) * 2) return false;
  std::size_t i = 0;
  auto readF32 = [&](f32& v) {
    if (i + sizeof(f32) > size) return false;
    std::memcpy(&v, data + i, sizeof(f32));
    i += sizeof(f32);
    return true;
  };
  auto readI32 = [&](i32& v) {
    if (i + sizeof(i32) > size) return false;
    std::memcpy(&v, data + i, sizeof(i32));
    i += sizeof(i32);
    return true;
  };
  if (!readF32(f.time))    return false;
  if (!readF32(f.ballX))   return false;
  if (!readF32(f.ballY))   return false;
  if (!readF32(f.ballVx))  return false;
  if (!readF32(f.ballVy))  return false;
  if (!readI32(f.homeScore)) return false;
  if (!readI32(f.awayScore)) return false;
  // Players are appended without count markers; the caller is expected to
  // know how many to read by passing a sized buffer. For this minimal API we
  // just consume the remaining bytes as player records.
  while (i + sizeof(u8) + sizeof(f32) * 4 + sizeof(u8) <= size) {
    ReplayFrame::PlayerSnap p;
    p.shirtNumber = data[i++];
    if (!readF32(p.x))  return false;
    if (!readF32(p.y))  return false;
    if (!readF32(p.vx)) return false;
    if (!readF32(p.vy)) return false;
    p.hasBall = data[i++] != 0;
    f.homePlayers.push_back(p);
  }
  return true;
}

}  // namespace kimia::street
