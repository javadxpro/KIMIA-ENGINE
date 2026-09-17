#include <kimia/PhotoMode.h>
#include <cstdio>
#include <cstring>

namespace kimia::street {

const char* photoFilterName(PhotoFilter f) {
  switch (f) {
    case PhotoFilter::None:     return "Original";
    case PhotoFilter::Sepia:   return "Sepia";
    case PhotoFilter::CoolBlue: return "Cool Blue";
    case PhotoFilter::Heat:     return "Heat";
    case PhotoFilter::Bw:       return "B/W";
    case PhotoFilter::Vignette: return "Vignette";
    case PhotoFilter::PopArt:   return "Pop Art";
  }
  return "?";
}

void applyFilter(PhotoCapture& photo, PhotoFilter filter) {
  if (filter == PhotoFilter::None) return;
  if (photo.rgba.empty()) return;
  const std::size_t n = photo.rgba.size();
  for (std::size_t i = 0; i < n; i += 4) {
    const u8 r = photo.rgba[i + 0];
    const u8 g = photo.rgba[i + 1];
    const u8 b = photo.rgba[i + 2];
    switch (filter) {
      case PhotoFilter::Sepia:
        photo.rgba[i + 0] = std::min(255, (r * 393 + g * 769 + b * 189) / 1000);
        photo.rgba[i + 1] = std::min(255, (r * 349 + g * 686 + b * 168) / 1000);
        photo.rgba[i + 2] = std::min(255, (r * 272 + g * 534 + b * 131) / 1000);
        break;
      case PhotoFilter::CoolBlue:
        photo.rgba[i + 0] = static_cast<u8>(r * 0.8f);
        photo.rgba[i + 1] = std::min(255, static_cast<int>(g * 1.0f));
        photo.rgba[i + 2] = std::min(255, static_cast<int>(b * 1.2f));
        break;
      case PhotoFilter::Heat:
        photo.rgba[i + 0] = std::min(255, static_cast<int>(r * 1.3f));
        photo.rgba[i + 1] = static_cast<u8>(g * 0.9f);
        photo.rgba[i + 2] = static_cast<u8>(b * 0.7f);
        break;
      case PhotoFilter::Bw: {
        const u8 gray = static_cast<u8>((r * 299 + g * 587 + b * 114) / 1000);
        photo.rgba[i + 0] = gray;
        photo.rgba[i + 1] = gray;
        photo.rgba[i + 2] = gray;
        break;
      }
      case PhotoFilter::Vignette: {
        // Vignette is position-based; here we apply a subtle darken.
        photo.rgba[i + 0] = static_cast<u8>(r * 0.85f);
        photo.rgba[i + 1] = static_cast<u8>(g * 0.85f);
        photo.rgba[i + 2] = static_cast<u8>(b * 0.85f);
        break;
      }
      case PhotoFilter::PopArt:
        photo.rgba[i + 0] = r > 127 ? 255 : 0;
        photo.rgba[i + 1] = g > 127 ? 255 : 0;
        photo.rgba[i + 2] = b > 127 ? 255 : 0;
        break;
      case PhotoFilter::None: break;
    }
  }
}

std::string makeCaption(const std::string& scorer,
                       const std::string& trick,
                       i32 scoreHome,
                       i32 scoreAway) {
  char buf[256];
  if (!scorer.empty() && !trick.empty()) {
    std::snprintf(buf, sizeof(buf),
                  "%s scores with a %s! (%d-%d)",
                  scorer.c_str(), trick.c_str(), scoreHome, scoreAway);
  } else if (!scorer.empty()) {
    std::snprintf(buf, sizeof(buf), "%s strikes! (%d-%d)",
                  scorer.c_str(), scoreHome, scoreAway);
  } else {
    std::snprintf(buf, sizeof(buf), "Match snapshot (%d-%d)",
                  scoreHome, scoreAway);
  }
  return buf;
}

}  // namespace kimia::street
