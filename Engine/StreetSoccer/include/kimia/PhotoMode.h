#pragma once
// =============================================================================
//  Photo Mode (Brazil Football Street / فوتبال خیابونی ایران)
//
//  Freezeframe capture of the current scene + filter pipeline. The captured
//  photo can be saved to disk or shared as a still image. Filters run in the
//  render layer; here we just declare the data and metadata.
//
//  Why: the single most viral content type for sports games is the
//  photo-shopped highlight screenshot. Players LOVE sharing them.
// =============================================================================

#include <kimia/Types.h>
#include <string>
#include <vector>

namespace kimia::street {

enum class PhotoFilter {
  None,
  Sepia,
  CoolBlue,
  Heat,        // warm tones, red-shift
  Bw,          // black & white
  Vignette,
  PopArt,
};

struct PhotoCapture {
  std::vector<u8> rgba;       // RGBA pixels, top-down row-major.
  i32 width  = 0;
  i32 height = 0;
  std::string caption;
  PhotoFilter filter = PhotoFilter::None;
  f32 freezeTime = 0.0f;     // match time at capture
  std::string scorerName;     // for player cards
  std::string trickName;      // for trick captures
};

// Apply a filter in-place. Pure CPU implementation; for production use GPU.
void applyFilter(PhotoCapture& photo, PhotoFilter filter);

const char* photoFilterName(PhotoFilter f);

// Build a caption from player + event info.
std::string makeCaption(const std::string& scorer,
                       const std::string& trick,
                       i32 scoreHome,
                       i32 scoreAway);

}  // namespace kimia::street
