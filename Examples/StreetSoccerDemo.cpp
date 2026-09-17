// StreetSoccerDemo — minimal headless demo that runs the AI vs AI match
// and renders the story.
//
//   kimia_street_soccer [--frames N] [--auto-kick] [--story] [--pitch]
//
// Default: 600 frames (10 simulated seconds) of AI vs AI.
//
//   --story    Render the StorySequence timeline via TermuxConsole at end.
//   --pitch    Render a single 60x14 ASCII pitch at start (verifies renderer).
//   --live     After every frame, render the ASCII pitch (slow).
//
// On Poco X3 Pro via Termux: just ./kimia_street_soccer --story and you
// get the full "Koye-Abouzar" narrative thread.

#include <kimia/StreetSoccerPhysics.h>
#include <kimia/StreetAIController.h>
#include <kimia/PlayerTraits.h>
#include <kimia/Physics.h>
#include <kimia/Types.h>
#include <kimia/StorySequence.h>
#include <kimia/TermuxConsole.h>
#include <kimia/TermuxInput.h>
#include <kimia/DensityMass.h>
#include <kimia/KimiaPhysics.h>
#include <kimia/CinematicCamera.h>
#include <kimia/CurrencySystem.h>
#include <kimia/GameMode.h>
#include <kimia/DialogueSystem.h>
#include <kimia/RandomEvents.h>
#include <kimia/DdaPolicy.h>
#include <kimia/StreetActions.h>
#include <kimia/HelicopterCamera.h>
#include <kimia/WebSnapshot.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <cmath>

using namespace kimia;        // brings in i32, i64, f32, ...
using namespace kimia::street;

namespace {

struct TeamAI {
  std::vector<PlayerTraits> traits;
};

void applyAIDecisions(MatchState& m,
                      StreetPhysicsBridge& b,
                      const TeamAI& home,
                      const TeamAI& away,
                      f32 dt) {
  auto driveTeam = [&](std::vector<Player>& team,
                       const std::vector<PlayerTraits>& traits,
                       TeamSide side) {
    for (std::size_t i = 0; i < team.size(); ++i) {
      if (i >= traits.size()) continue;
      const AIDecision d = tickAI(m, team[i], traits[i],
                                  static_cast<i32>(i), side, dt);
      pushPlayer(b, team[i].shirtNumber, d.desiredVx, d.desiredVy);
      if (d.wantsKick) {
        const f32 kx = d.desiredVx * d.kickPower * 0.4f;
        const f32 ky = d.desiredVy * d.kickPower * 0.4f;
        const f32 dx = team[i].positionX - m.ball.x;
        const f32 dy = team[i].positionY - m.ball.y;
        if (std::sqrt(dx*dx + dy*dy) < 1.5f) {
          kickBall(b, kx, ky);
        }
      }
    }
  };
  driveTeam(m.homeTeam, home.traits, TeamSide::Home);
  driveTeam(m.awayTeam, away.traits, TeamSide::Away);
}

}  // namespace

int main(int argc, char** argv) {
  i32 frames = 600;        // 10 sec @ 60Hz
  bool autoKick = true;
  bool doStory  = false;
  bool doPitch  = false;
  bool doLive   = false;
  bool doInteractive = false;
  // Force-away-goals: scripted test mode that guarantees the home team
  // ends down by exactly 2 — used to trigger the Comeback Burst for
  // testing the StorySequence path.
  i32 scriptAwayGoals = -1;
  bool doShowcase = false;
  GameMode initialMode = GameMode::Street;
  [[maybe_unused]] DialogueLevel dialogLvl = DialogueLevel::Medium;
  bool doWeb = false;
  // Allow callers to redirect snapshot output. Default falls back to
  // /tmp/kimia-web which fails on read-only /tmp environments; the
  // Tools/run_webview.sh launcher sets KIMIA_WEB_OUT before exec'ing us.
  std::string webDir = "/tmp/kimia-web";
  if (const char* env = std::getenv("KIMIA_WEB_OUT")) {
    webDir = env;
  }
  bool doTrailer = false;
  bool doLoop    = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--frames" && i + 1 < argc) {
      frames = std::atoi(argv[++i]);
    } else if (arg == "--auto-kick") {
      autoKick = true;
    } else if (arg == "--no-kick") {
      autoKick = false;
    } else if (arg == "--story") {
      doStory = true;
    } else if (arg == "--pitch") {
      doPitch = true;
    } else if (arg == "--live") {
      doLive = true;
    } else if (arg == "--force-away-2" && i + 1 < argc) {
      scriptAwayGoals = std::atoi(argv[++i]);
    } else if (arg == "--interactive" || arg == "-i") {
      doInteractive = true;
    } else if (arg == "--showcase") {
      doShowcase = true;
      doStory = true;
    } else if (arg == "--mode" && i + 1 < argc) {
      const std::string m = argv[++i];
      if      (m == "street") initialMode = GameMode::Street;
      else if (m == "comedy") initialMode = GameMode::Comedy;
      else if (m == "grass")  initialMode = GameMode::Grass;
      else if (m == "speed")  initialMode = GameMode::Speed;
    } else if (arg == "--difficulty" && i + 1 < argc) {
      const std::string d = argv[++i];
      if      (d == "easy")   dialogLvl = DialogueLevel::Easy;
      else if (d == "medium") dialogLvl = DialogueLevel::Medium;
      else if (d == "hard")   dialogLvl = DialogueLevel::Hard;
    } else if (arg == "--web" || arg == "--webviewer") {
      doWeb = true;
    } else if (arg == "--trailer") {
      doTrailer = true;
      // Trailer cues run 0..21s; default 10s is too short. Bump unless
      // --frames was given explicitly (we honour the user's choice then).
      if (frames == 600) frames = 1800;  // 30s headroom for the script
    } else if (arg == "--loop") {
      doLoop = true;
    }
  }

  MatchState m;
  buildDefaultTeams(m);
  m.playing = true;

  kimia::PhysicsWorld world(1.0 / 60.0, 5);
  StreetPhysicsBridge bridge;
  setupStreetPhysics(bridge, world, m.pitch);

  spawnBallBody(bridge, world, m.ball);
  for (auto& pl : m.homeTeam) spawnPlayerBody(bridge, world, pl);
  for (auto& pl : m.awayTeam) spawnPlayerBody(bridge, world, pl);

  TeamAI homeAI;
  for (const auto& pl : m.homeTeam) {
    homeAI.traits.push_back(defaultTraitsForSide(TeamSide::Home,
                                                 pl.shirtNumber));
  }
  TeamAI awayAI;
  for (const auto& pl : m.awayTeam) {
    awayAI.traits.push_back(defaultTraitsForSide(TeamSide::Away,
                                                 pl.shirtNumber));
  }

  // Story sequence (optional).
  StorySequence story;
  story.bind(&m, &homeAI.traits, &awayAI.traits);

  std::printf("Street Soccer Demo\n");
  std::printf("Frames: %d (%.1f simulated seconds)\n",
              frames, frames / 60.0f);
  std::printf("Teams: Home=%zu players, Away=%zu players\n",
              m.homeTeam.size(), m.awayTeam.size());
  std::printf("Pitch: %.1fm x %.1fm\n", m.pitch.length, m.pitch.width);

  // Showcase: print all phase-1 systems status.
  if (doShowcase) {
    std::printf("\n═══ PHASE-1 SYSTEM SHOWCASE ═══\n");

    // Density-based masses (real materials).
    std::printf("\n[1] Density-based mass:\n");
    std::printf("    Football ball:   %.2f kg (FIFA size 5 = 0.43 kg)\n",
                footballBallMass());
    std::printf("    Tin bottle:      %.2f kg\n", tinBottleMass());
    std::printf("    Steel bollard:   %.2f kg (the post)\n", steelBollardMass());
    std::printf("    Kid body (torso):%.2f kg\n", kidMassApprox());

    // DDA policy
    DdaPolicy pol;
    std::printf("\n[2] DDA policy: %s\n", pol.description());

    // Game mode tuning
    std::printf("\n[3] Game modes:\n");
    for (GameMode mm : {GameMode::Street, GameMode::Comedy,
                        GameMode::Grass, GameMode::Speed}) {
      ModeTuning tn = tuningFor(mm);
      std::printf("    %-12s | grav=%.2f bounce=%.2f sprint=%.2f dt=%.0fHz\n",
                  modeLabel(mm), tn.gravityScale, tn.ballRestitution,
                  tn.sprintMultiplier, 1.0f / tn.fixedDt);
    }

    // Currency tiers
    std::printf("\n[4] Currency computation (skill=0.4, 1 goal, won):\n");
    {
      Wallet w;
      CurrencyAward a = computeAward(0.4f, 1, true, false);
      applyAward(w, a);
      std::printf("    Coin=%u Token=%u Gem=%u Star=%u\n",
                  w.coin, w.token, w.gem, w.star);
    }
    std::printf("    Currency tiers coin=participation + per-goal\n"
                "    gem=skill-gated (≥0.5), star=top tier win+≥0.75\n");

    // Random events
    std::printf("\n[5] Random events for a 600s match (seed=42):\n");
    {
      auto evs = generateRandomEvents(42u, 600.0f);
      std::printf("    %zu events scheduled\n", evs.size());
      for (size_t i = 0; i < evs.size() && i < 6; ++i) {
        std::printf("    t=%4.0fs  %s (%s)\n",
                    evs[i].durationSeconds, eventLabelFa(evs[i].kind),
                    eventLabel(evs[i].kind));
      }
    }
  }

  [[maybe_unused]] ModeTuning tune = tuningFor(initialMode);
  std::printf("\nMode: %s %s%s\n",
              modeLabel(initialMode),
              autoKick ? "[AI]" : "[no-kick]",
              doStory ? " +story" : "");
  std::printf("────\n");
  std::printf("---\n");

  if (doPitch) {
    TermuxConsole tc;
    tc.renderPitch(m);
  }

  i32 lastHome = 0;
  i32 lastAway = 0;
  TermuxConsole tc;
  TermuxInput inp;
  HelicopterCamera heliCam;
  WebSnapshot webSnap;
  if (doWeb) {
    webSnap.init(webDir);
    webSnap.cam = heliCam;  // initial state
  }
  if (doInteractive) inp.enterRawMode();

  // -------- Trailer script: a sequence of StreetActions driven by the
  // engine so the showcase stays deterministic and watchable.
  struct TrailerCue {
    f32 atSeconds;             // start time within the trailer
    StreetActionKind kind;     // which action
    bool nearGoal;             // action ends near a goal mouth
  };
  std::vector<TrailerCue> trailerCues;
  if (doTrailer) {
    trailerCues = {
      { 0.0f,  StreetActionKind::GroundCross,       false},
      { 2.5f,  StreetActionKind::BackheelTap,       false},
      { 4.0f,  StreetActionKind::FirstTimeVolley,    true },
      { 6.5f,  StreetActionKind::ChipPass,           false},
      { 8.0f,  StreetActionKind::BicycleHeader,      true },
      {11.0f,  StreetActionKind::KeeperWallGoal,     true },
      {14.0f,  StreetActionKind::SombreroFlick,      false},
      {16.0f,  StreetActionKind::TallManNutmeg,      true },
      {18.5f,  StreetActionKind::FreeKickTopCorner,  true },
      {21.0f,  StreetActionKind::WheelBreakDribble,  false},
    };
  }
  std::vector<ActionStep> currentActionSteps;
  f32 actionElapsed  = 0;
  i32 nextCue = 0;
  std::string currentStoryHeadline;
  u32 rngState = 0xC0FFEEu;

  // Loop mode: re-run the trailer cues indefinitely until SIGTERM. Used
  // by Tools/run_webview.sh so the WebViewer keeps streaming even after
  // the first scripted demo finishes.
  i32 effectiveFrames = frames;
  if (doLoop && doTrailer) effectiveFrames = 1 << 30;  // ~big

  for (i32 i = 0; i < effectiveFrames; ++i) {
  const f32 dt = 1.0f / 60.0f;

    // Trailer: feed scripted action impulses to ball / players.
    if (doTrailer && nextCue < static_cast<i32>(trailerCues.size()) &&
        m.matchTime >= trailerCues[nextCue].atSeconds) {
      // Recentre the ball before each cue only if it's currently pinned
      // to a wall corner (|x|≈14 or |y|≈7). Otherwise we let the ball
      // continue from where the previous cue left it.
      if (doLoop && doTrailer &&
          (std::fabs(m.ball.x) > 13.0f || std::fabs(m.ball.y) > 6.0f)) {
        m.ball.x = 0.0f; m.ball.y = 0.0f;
        m.ball.vx = 0.0f; m.ball.vy = 0.0f;
      }
      const auto& cue = trailerCues[nextCue];
      currentActionSteps = rollStreetAction(cue.kind, 0.85f, &rngState);
      if (currentActionSteps.empty()) {
        currentActionSteps = rollStreetAction(cue.kind, 0.65f, &rngState);
      }
      actionElapsed = 0;
      currentStoryHeadline = std::string(actionLabelFa(cue.kind));
      nextCue++;
      std::printf("[Trailer] %.1fs cue: %s\n",
                  m.matchTime, actionLabelEn(cue.kind));
    }

    if (!currentActionSteps.empty()) {
      // Find step matching current elapsed time.
      for (const auto& step : currentActionSteps) {
        if (actionElapsed >= step.at && step.at >= 0) {
          // Cap impulse magnitudes so the WebViewer stays legible.
          auto cap = [](float v) { return std::fmax(-9.0f, std::fmin(9.0f, v)); };
          if (step.toBall) {
            kickBall(bridge, cap(step.impulseX), cap(step.impulseY));
          } else if (step.playerIndex < m.homeTeam.size()) {
            pushPlayer(bridge,
                       m.homeTeam[step.playerIndex].shirtNumber,
                       cap(step.impulseX), cap(step.impulseZ));
          }
        }
      }
      actionElapsed += dt;

    // Trailer loop: when all cues have fired and loop is requested,
    // restart the script so the WebViewer keeps showing fresh action.
    // We rebuild the match from scratch — otherwise the ball stays
    // clamped at a wall corner and the snapshot freezes.
    if (doLoop && doTrailer &&
        nextCue >= static_cast<i32>(trailerCues.size()) &&
        currentActionSteps.empty()) {
      buildDefaultTeams(m);  // resets ball + players + scores
      // Force ball back to centre — physics sometimes leaves it stuck
      // on a wall even after the bridge respawn, so reset explicitly.
      m.ball.x = 0.0f; m.ball.y = 0.0f;
      m.ball.vx = 0.0f; m.ball.vy = 0.0f;
      spawnBallBody(bridge, world, m.ball);
      for (auto& pl : m.homeTeam) spawnPlayerBody(bridge, world, pl);
      for (auto& pl : m.awayTeam) spawnPlayerBody(bridge, world, pl);
      story.bind(&m, &homeAI.traits, &awayAI.traits);
      nextCue = 0;
      actionElapsed = 0;
      currentActionSteps.clear();
      currentStoryHeadline.clear();
      lastHome = 0;
      lastAway = 0;
      rngState = 0xC0FFEEu ^ static_cast<u32>(i);  // vary the script
      std::printf("[Trailer] loop restart @ frame %d\n", i);
    }

      // Did the last step trigger a goal?
      if (currentActionSteps.back().triggersGoal &&
          actionElapsed > currentActionSteps.back().at &&
          m.awayScore == lastAway && m.homeScore == lastHome) {
        // Force a goal: home = award a goal about now.
        m.homeScore += 1;
        GoalEvent ge;
        ge.scoredBy = TeamSide::Home;
        ge.timeSeconds = m.matchTime;
        m.goals.push_back(ge);
        lastHome = m.homeScore;
        std::printf("[Trailer] GOAL! %.1fs\n", m.matchTime);
        // After a goal the ball is pinned at the goal mouth; recentre it
        // so the next cue has somewhere to start from.
        m.ball.x = 0.0f; m.ball.y = 0.0f;
        m.ball.vx = 0.0f; m.ball.vy = 0.0f;
        currentActionSteps.clear();
      }

      if (!currentActionSteps.empty() &&
          actionElapsed > currentActionSteps.back().at + 0.05f) {
        currentActionSteps.clear();
      }
    } else if (!doInteractive && !(doLoop && doTrailer)) {
      // In trailer-loop mode the cue script drives both ball and
      // players — letting AI also push them pins everyone to the
      // pitch walls within a couple of seconds. Skip AI entirely.
      applyAIDecisions(m, bridge, homeAI, awayAI, dt);
    }

    // Clamp runaway physics values inside the pitch envelope. The trailer
    // scripts can push impulses that exceed goalkeeper X; we wrap around
    // to keep the WebViewer scene legible.
    {
      const f32 maxPos = 14.0f;   // pitch half-length
      const f32 maxY   = 7.0f;    // pitch half-width
      const f32 maxVel = 15.0f;   // m/s
      if (m.ball.x > maxPos) m.ball.x = maxPos - 1.0f;
      if (m.ball.x < -maxPos) m.ball.x = -maxPos + 1.0f;
      if (m.ball.y > maxY || m.ball.y < -maxY) m.ball.y = 0.0f;
      if (m.ball.vx >  maxVel) m.ball.vx =  maxVel;
      if (m.ball.vx < -maxVel) m.ball.vx = -maxVel;
      if (m.ball.vy >  maxVel) m.ball.vy =  maxVel;
      if (m.ball.vy < -maxVel) m.ball.vy = -maxVel;
    }

    if (doInteractive && !m.homeTeam.empty()) {
      // Player 0 of home team is human-controlled.
      Intent intent = inp.pollOnce();
      Player& human = m.homeTeam[0];
      if (intent.desiredVx != 0 || intent.desiredVy != 0 ||
          intent.wantsKick || intent.wantsTrick) {
        pushPlayer(bridge, human.shirtNumber,
                   intent.desiredVx * 4.0f, intent.desiredVy * 4.0f);
        if (intent.wantsKick) {
          // kick toward goal (positive X)
          kickBall(bridge, 8.0f, 0.0f);
        }
      }
    } else {
      // No human player — drive everyone via AI.
      // (Already applied above via applyAIDecisions.)
    }

    tickMatch(m, bridge, 1.0f / 60.0f);
    story.update(1.0f / 60.0f);
    // In trailer-loop mode the cue shots pin the ball at a wall corner
    // with low velocity. Between cues, give the ball a steady orbit
    // around the centre so the chase camera keeps moving and the
    // WebViewer scene doesn't look frozen. Skip during a cue so the
    // user sees the cue shots play out.
    if (doLoop && doTrailer && currentActionSteps.empty()) {
      const f32 t = m.matchTime;
      m.ball.x = std::cos(t * 0.6f) * 6.0f;
      m.ball.y = std::sin(t * 0.6f) * 3.0f;
      m.ball.vx = -std::sin(t * 0.6f) * 3.6f;
      m.ball.vy =  std::cos(t * 0.6f) * 1.8f;
    }

    // Hard clamp ball to keep WebViewer scene legible for trailer mode.
    {
      constexpr f32 maxPos = 14.0f;
      constexpr f32 maxY   = 7.0f;
      if (m.ball.x >  maxPos) m.ball.x =  maxPos;
      if (m.ball.x < -maxPos) m.ball.x = -maxPos;
      if (m.ball.y >  maxY) m.ball.y =  maxY;
      if (m.ball.y < -maxY) m.ball.y = -maxY;
      if (m.ball.vx > 12.0f) m.ball.vx =  12.0f;
      if (m.ball.vx < -12.0f) m.ball.vx = -12.0f;
      if (m.ball.vy >  8.0f) m.ball.vy =  8.0f;
      if (m.ball.vy < -8.0f) m.ball.vy = -8.0f;
    }
    // (No-op: in loop+trailer mode the ball stays where physics puts it.
    // The goal handler above recentres after every score so the WebViewer
    // keeps moving. Resetting on every idle frame hid the cue shots.)
    if (m.homeScore != lastHome || m.awayScore != lastAway) {
      std::printf("Frame %5d (t=%6.1fs): GOAL! Home=%d Away=%d\n",
                  i, m.matchTime, m.homeScore, m.awayScore);
      lastHome = m.homeScore;
      lastAway = m.awayScore;
    }
    if (scriptAwayGoals >= 0 && m.awayScore < scriptAwayGoals
        && (i % 60 == 0)) {
      // scripted force-away-goal for StorySequence testing
      m.awayScore += 1;
      GoalEvent ge;
      ge.scoredBy   = TeamSide::Away;
      ge.timeSeconds = m.matchTime;
      m.goals.push_back(ge);
      std::printf("  [scripted] Away goal awarded at t=%.1fs (Away now %d)\n",
                  m.matchTime, m.awayScore);
      lastAway = m.awayScore;
    }
    if (doLive && (i % 30 == 0)) {
      // don't fill the screen — re-render pitch every 0.5s
    }

    if (doInteractive && (i % 6 == 0)) {  // 10 Hz pitch repaint
      tc.clearScreen();
      tc.renderPitch(m);
      std::printf("\n  t=%.1fs  Home %d - Away %d\n"
                  "  WASD/Arrows move • SPACE kick • K trick • Q quit\n",
                  m.matchTime, m.homeScore, m.awayScore);
      std::fflush(stdout);
      // crude pacing — 6 frames at 60Hz = 0.1s
      // (Termux stdin is line-buffered; this keeps it ~playable)
    }

    // Helicopter camera + WebViewer snapshot.
    {
      heliCam.update(dt, Vec3(m.ball.x, m.ball.y, 0));
      if (doWeb && (i % 2 == 0)) {  // 30 Hz snapshot
        webSnap.cam = heliCam;
        webSnap.write(m, currentStoryHeadline, i);
      }
    }
  }
  if (doInteractive) inp.leaveRawMode();
  (void)rngState;

  std::printf("---\n");
  std::printf("Final: Home %d - Away %d\n", m.homeScore, m.awayScore);
  std::printf("Time: %.1fs, Goals: %zu\n", m.matchTime, m.goals.size());

  if (doStory) {
    TermuxConsole tc;
    tc.playStory(story, m, homeAI.traits, awayAI.traits);
  }
  return 0;
}
