#include <kimia_test.h>
#include <kimia/ReplaySystem.h>
#include <cstdio>

using namespace kimia::street;

static int g_pass = 0;
static int g_fail = 0;
#define EXPECT(cond) do { \
  if (cond) { ++g_pass; } \
  else      { ++g_fail; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } \
} while (0)
#define EXPECT_EQ(a, b) do { \
  auto va = (a); auto vb = (b); \
  if (va == vb) { ++g_pass; } \
  else { ++g_fail; std::printf("FAIL %s:%d %s == %s\n", __FILE__, __LINE__, #a, #b); } \
} while (0)

KIMIA_TEST(Replay_BeginClearsAndSetsSeed) {
  ReplayRecorder r;
  r.begin(42);
  EXPECT(r.isRecording());
  EXPECT_EQ(r.seed(), 42u);
  EXPECT_EQ(r.frameCount(), 0u);
  r.end();
}

KIMIA_TEST(Replay_CaptureFrameAddsEntry) {
  MatchState m;
  buildDefaultTeams(m);
  ReplayRecorder r;
  r.begin(1);
  r.captureFrame(m, 0.0f);
  r.captureFrame(m, 1.0f);
  EXPECT_EQ(r.frameCount(), 2u);
}

KIMIA_TEST(Replay_EndStopsRecording) {
  ReplayRecorder r;
  r.begin(1);
  r.end();
  EXPECT(!r.isRecording());
}

KIMIA_TEST(Replay_CaptureAfterEndIsSafe) {
  MatchState m;
  buildDefaultTeams(m);
  ReplayRecorder r;
  r.begin(1);
  r.captureFrame(m, 0.0f);
  r.end();
  const auto before = r.frameCount();
  r.captureFrame(m, 1.0f);
  EXPECT_EQ(r.frameCount(), before);
}

KIMIA_TEST(Replay_BallPositionRecorded) {
  MatchState m;
  buildDefaultTeams(m);
  m.ball.x = 5.0f;
  m.ball.y = -2.0f;
  ReplayRecorder r;
  r.begin(1);
  r.captureFrame(m, 0.0f);
  ReplayFrame f;
  EXPECT(r.frameAt(0.0f, f));
  EXPECT(f.ballX == 5.0f);
  EXPECT(f.ballY == -2.0f);
}

KIMIA_TEST(Replay_DurationComputed) {
  MatchState m;
  buildDefaultTeams(m);
  ReplayRecorder r;
  r.begin(1);
  r.captureFrame(m, 0.0f);
  r.captureFrame(m, 5.0f);
  EXPECT(r.duration() == 5.0f);
}

KIMIA_TEST(Replay_MaxFrameCap) {
  MatchState m;
  buildDefaultTeams(m);
  ReplayRecorder r(5);
  r.begin(1);
  for (int i = 0; i < 20; ++i) r.captureFrame(m, static_cast<float>(i));
  EXPECT_EQ(r.frameCount(), 5u);
}

KIMIA_TEST(Replay_FrameAtOutOfRange) {
  MatchState m;
  buildDefaultTeams(m);
  ReplayRecorder r;
  r.begin(1);
  r.captureFrame(m, 1.0f);
  r.captureFrame(m, 2.0f);
  ReplayFrame f;
  EXPECT(!r.frameAt(0.0f, f));
  EXPECT(!r.frameAt(5.0f, f));
}

KIMIA_TEST(Replay_FrameAtInRange) {
  MatchState m;
  buildDefaultTeams(m);
  ReplayRecorder r;
  r.begin(1);
  r.captureFrame(m, 1.0f);
  r.captureFrame(m, 2.0f);
  r.captureFrame(m, 3.0f);
  ReplayFrame f;
  EXPECT(r.frameAt(2.0f, f));
}

KIMIA_TEST(Replay_EmptyDuration) {
  ReplayRecorder r;
  EXPECT(r.duration() == 0.0f);
}

// ---------------------------------------------------------------------------
KIMIA_TEST(ReplayPlayer_BeginSetsState) {
  MatchState m;
  buildDefaultTeams(m);
  ReplayRecorder r;
  r.begin(1);
  r.captureFrame(m, 0.0f);
  r.captureFrame(m, 1.0f);
  r.captureFrame(m, 2.0f);
  r.end();
  ReplayPlayer p;
  p.begin(r, 1.0f);
  EXPECT(p.isPlaying());
  EXPECT_EQ(p.speed(), 1.0f);
}

KIMIA_TEST(ReplayPlayer_TickAdvancesTime) {
  MatchState m;
  buildDefaultTeams(m);
  ReplayRecorder r;
  r.begin(1);
  for (int i = 0; i < 10; ++i) r.captureFrame(m, static_cast<float>(i));
  r.end();
  ReplayPlayer p;
  p.begin(r, 1.0f);
  const kimia::f32 before = p.time();
  p.tick(0.5f);
  EXPECT(p.time() > before);
}

KIMIA_TEST(ReplayPlayer_TickDoubleSpeed) {
  MatchState m;
  buildDefaultTeams(m);
  ReplayRecorder r;
  r.begin(1);
  for (int i = 0; i < 10; ++i) r.captureFrame(m, static_cast<float>(i));
  r.end();
  ReplayPlayer p;
  p.begin(r, 2.0f);
  p.tick(1.0f);
  EXPECT(p.time() > 1.5f);
}

KIMIA_TEST(ReplayPlayer_StopsAtEnd) {
  MatchState m;
  buildDefaultTeams(m);
  ReplayRecorder r;
  r.begin(1);
  r.captureFrame(m, 0.0f);
  r.captureFrame(m, 1.0f);
  r.end();
  ReplayPlayer p;
  p.begin(r, 1.0f);
  for (int i = 0; i < 100; ++i) p.tick(1.0f);
  EXPECT(!p.isPlaying());
}

KIMIA_TEST(ReplayPlayer_CurrentFrameMatchesTime) {
  MatchState m;
  buildDefaultTeams(m);
  m.ball.x = 3.0f;
  ReplayRecorder r;
  r.begin(1);
  r.captureFrame(m, 0.0f);
  m.ball.x = 5.0f;
  r.captureFrame(m, 1.0f);
  r.end();
  ReplayPlayer p;
  p.begin(r, 1.0f);
  p.tick(0.5f);
  ReplayFrame f;
  EXPECT(p.currentFrame(f));
  EXPECT(f.time == 0.5f);
}

KIMIA_TEST(ReplayPlayer_NoRecordSafe) {
  ReplayPlayer p;
  EXPECT(!p.isPlaying());
  p.tick(1.0f);
  ReplayFrame f;
  EXPECT(!p.currentFrame(f));
}

KIMIA_TEST(ReplayPlayer_SetSpeed) {
  MatchState m;
  buildDefaultTeams(m);
  ReplayRecorder r;
  r.begin(1);
  for (int i = 0; i < 5; ++i) r.captureFrame(m, static_cast<float>(i));
  r.end();
  ReplayPlayer p;
  p.begin(r, 1.0f);
  p.setSpeed(0.5f);
  EXPECT(p.speed() == 0.5f);
}

// ---------------------------------------------------------------------------
KIMIA_TEST(Serialize_FrameRoundtrip) {
  MatchState m;
  buildDefaultTeams(m);
  m.ball.x = 7.0f;
  m.ball.y = -3.0f;
  ReplayRecorder r;
  r.begin(1);
  r.captureFrame(m, 0.0f);
  const auto& frame = r.frames().front();
  const auto bytes = serializeFrame(frame);
  EXPECT(!bytes.empty());
  ReplayFrame out;
  EXPECT(deserializeFrame(bytes.data(), bytes.size(), out));
  EXPECT(out.ballX == 7.0f);
  EXPECT(out.ballY == -3.0f);
}

KIMIA_TEST(Serialize_BadBuffer) {
  ReplayFrame f;
  EXPECT(!deserializeFrame(nullptr, 0, f));
  const kimia::u8 bad[3] = {0, 0, 0};
  EXPECT(!deserializeFrame(bad, 3, f));
}

KIMIA_TEST(Serialize_PreservesPlayers) {
  MatchState m;
  buildDefaultTeams(m);
  ReplayRecorder r;
  r.begin(1);
  r.captureFrame(m, 0.0f);
  const auto& frame = r.frames().front();
  const auto bytes = serializeFrame(frame);
  ReplayFrame out;
  EXPECT(deserializeFrame(bytes.data(), bytes.size(), out));
  EXPECT(!out.homePlayers.empty());
  // Player 1 (Pelezinho) should be in the home list.
  bool found = false;
  for (const auto& p : out.homePlayers) {
    if (p.shirtNumber == 1) found = true;
  }
  EXPECT(found);
}
