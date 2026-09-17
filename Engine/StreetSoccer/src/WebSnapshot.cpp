// WebSnapshot.cpp — file-based bridge between the engine and the web viewer.

#include <kimia/WebSnapshot.h>

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace kimia::street {

bool WebSnapshot::init(const std::string& outDir) {
  dir = outDir;
  std::error_code ec;
  fs::create_directories(outDir, ec);
  return !ec;
}

void WebSnapshot::write(const MatchState& match,
                        const std::string& recentStoryHeadlineFa,
                        i32 /*frame*/) {
  if (dir.empty()) return;

  std::string tmp  = dir + "/state.json.tmp";
  std::string path = dir + "/state.json";
  std::ofstream out(tmp, std::ios::trunc);
  if (!out) return;

  out << "{"
      << "\"time\":"     << match.matchTime << ","
      << "\"homeScore\":" << match.homeScore << ","
      << "\"awayScore\":" << match.awayScore << ","
      << "\"ball\":{\"x\":" << match.ball.x
      << ",\"y\":" << match.ball.y
      << ",\"vx\":" << match.ball.vx
      << ",\"vy\":" << match.ball.vy << "},";

  // Players
  out << "\"players\":[";
  bool first = true;
  for (const auto& pl : match.homeTeam) {
    if (!first) out << ",";
    first = false;
    out << "{\"side\":\"H\",\"n\":" << (int)pl.shirtNumber
        << ",\"x\":" << pl.positionX
        << ",\"y\":" << pl.positionY
        << ",\"name\":\"H" << (int)pl.shirtNumber << "\"}";
  }
  for (const auto& pl : match.awayTeam) {
    if (!first) out << ",";
    first = false;
    out << "{\"side\":\"A\",\"n\":" << (int)pl.shirtNumber
        << ",\"x\":" << pl.positionX
        << ",\"y\":" << pl.positionY
        << ",\"name\":\"A" << (int)pl.shirtNumber << "\"}";
  }
  out << "],";

  // Story headline (if any)
  out << "\"storyHeadline\":\"" << recentStoryHeadlineFa << "\",";

  // Camera
  out << "\"cam\":" << cam.snapshotJson() << "}";

  out.close();
  // atomic rename
  fs::rename(tmp, path);
}

}  // namespace kimia::street
