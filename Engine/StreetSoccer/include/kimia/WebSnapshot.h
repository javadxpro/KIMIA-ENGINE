#pragma once
// =============================================================================
//  WebSnapshot — write a JSON file the WebViewer can read.
//
//  The WebViewer is a plain HTML+JS page in Tools/web/index.html. It polls
//  /snap/state.json every 33ms (~30fps) via fetch() and renders the camera
//  + ball position. We use the *filesystem* as the IPC layer, which means
//  the page can be served from any static HTTP server (we spawn one) and
//  no fancy WebSocket setup is needed. The polling latency is low enough
//  to feel real-time at 30fps.
// =============================================================================

#include <kimia/StreetSoccer.h>
#include <kimia/HelicopterCamera.h>
#include <string>

namespace kimia::street {

struct WebSnapshot {
  std::string dir;  // output directory
  HelicopterCamera cam;
  u32    lastWriteFrame = 0;

  // Initialise. Creates `dir` if missing.
  bool init(const std::string& outDir);

  // Write a JSON snapshot of:
  //   - ball position + velocity
  //   - camera state (eye/lookAt/fov/mode/...)
  //   - home/away score
  //   - the most recent StoryEvent (if any)
  //   - elapsed match time
  // The snapshot is written to <dir>/state.json atomically (via tmp rename).
  void write(const MatchState& match,
             const std::string& recentStoryHeadlineFa,
             i32 frame);
};

}  // namespace kimia::street
