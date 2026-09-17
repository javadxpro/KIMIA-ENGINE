#include <kimia/StreetSoccer.h>

namespace kimia::street {

void resetMatch(MatchState& m) {
  m.ball.x = 0.0f;
  m.ball.y = 0.0f;
  m.ball.vx = 0.0f;
  m.ball.vy = 0.0f;
  m.matchTime = 0.0f;
  m.homeScore = 0;
  m.awayScore = 0;
  m.playing = false;
  m.goals.clear();

  // Reset player positions to a 4v4 street layout.
  auto resetSide = [&](std::vector<Player>& team, TeamSide side) {
    const f32 xStart = (side == TeamSide::Home)
        ? -m.pitch.length * 0.5f + 2.0f
        :  m.pitch.length * 0.5f - 2.0f;
    for (std::size_t i = 0; i < team.size(); ++i) {
      Player& p = team[i];
      p.team = side;
      p.hasBall = false;
      p.stamina = 100.0f;
      p.velocityX = 0.0f;
      p.velocityY = 0.0f;
      // 1 GK + 3 field players per side.
      if (p.isGoalkeeper) {
        p.positionX = (side == TeamSide::Home)
            ? -m.pitch.length * 0.5f + 0.5f
            :  m.pitch.length * 0.5f - 0.5f;
        p.positionY = 0.0f;
      } else {
        const i32 idx = static_cast<i32>(i == 0 ? 1 : (i - 1));
        // Spread 3 field players in an arc.
        const f32 offsets[3] = {0.0f, -4.0f, 4.0f};
        p.positionX = xStart + offsets[idx];
        p.positionY = (idx < 3) ? offsets[idx] : 0.0f;
      }
    }
  };
  resetSide(m.homeTeam, TeamSide::Home);
  resetSide(m.awayTeam, TeamSide::Away);
}

void tickBall(Ball& b, f32 dt) {
  // Linear drag (street ball on asphalt).
  constexpr f32 drag = 0.985f;
  b.x  += b.vx * dt;
  b.y  += b.vy * dt;
  b.vx *= drag;
  b.vy *= drag;
}

void detectGoals(MatchState& m, std::vector<GoalEvent>& newGoals) {
  const f32 goalX = m.pitch.length * 0.5f;
  const f32 halfGW = m.pitch.goalWidth * 0.5f;
  const Ball& b = m.ball;

  // Home team scores if ball crosses +X line within goal width.
  if (b.x >  goalX && std::abs(b.y) <= halfGW) {
    GoalEvent g;
    g.scoredBy = TeamSide::Home;
    g.timeSeconds = m.matchTime;
    newGoals.push_back(g);
  }
  // Away team scores if ball crosses -X line within goal width.
  if (b.x < -goalX && std::abs(b.y) <= halfGW) {
    GoalEvent g;
    g.scoredBy = TeamSide::Away;
    g.timeSeconds = m.matchTime;
    newGoals.push_back(g);
  }
}

void buildDefaultTeams(MatchState& m) {
  m.homeTeam.clear();
  m.awayTeam.clear();

  auto addPlayer = [](std::vector<Player>& team, const char* name,
                      u8 number, bool gk) {
    Player p;
    p.name = name;
    p.shirtNumber = number;
    p.isGoalkeeper = gk;
    team.push_back(p);
  };

  // Home team (Brazil-themed names; localized at render layer).
  addPlayer(m.homeTeam, "Pelezinho", 1, true);
  addPlayer(m.homeTeam, "Garrincha", 7, false);
  addPlayer(m.homeTeam, "Ronaldinho", 10, false);
  addPlayer(m.homeTeam, "Neymar Jr", 11, false);

  // Away team (Iran-themed names).
  addPlayer(m.awayTeam, "Haghighi",  1,  true);
  addPlayer(m.awayTeam, "Mahdavikia", 9,  false);
  addPlayer(m.awayTeam, "Daei",     10, false);
  addPlayer(m.awayTeam, "Azmoun",   23, false);

  resetMatch(m);
}

}  // namespace kimia::street
