#include <kimia_test.h>
#include <kimia/PhotoMode.h>
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

KIMIA_TEST(Photo_FilterNames) {
  EXPECT(std::string(photoFilterName(PhotoFilter::None)) == "Original");
  EXPECT(std::string(photoFilterName(PhotoFilter::Sepia)) == "Sepia");
  EXPECT(std::string(photoFilterName(PhotoFilter::CoolBlue)) == "Cool Blue");
  EXPECT(std::string(photoFilterName(PhotoFilter::Heat)) == "Heat");
  EXPECT(std::string(photoFilterName(PhotoFilter::Bw)) == "B/W");
  EXPECT(std::string(photoFilterName(PhotoFilter::Vignette)) == "Vignette");
  EXPECT(std::string(photoFilterName(PhotoFilter::PopArt)) == "Pop Art");
}

KIMIA_TEST(Photo_FilterNoneSafe) {
  PhotoCapture p;
  applyFilter(p, PhotoFilter::None);
  EXPECT(p.rgba.empty());
}

KIMIA_TEST(Photo_FilterOnEmptySafe) {
  PhotoCapture p;
  applyFilter(p, PhotoFilter::Sepia);
  EXPECT(p.rgba.empty());
}

KIMIA_TEST(Photo_SepiaChangesPixels) {
  PhotoCapture p;
  p.rgba = {100, 150, 200, 255, 50, 80, 110, 255};
  PhotoCapture before = p;
  applyFilter(p, PhotoFilter::Sepia);
  EXPECT(p.rgba != before.rgba);
}

KIMIA_TEST(Photo_BwProducesGray) {
  PhotoCapture p;
  p.rgba = {100, 150, 200, 255};
  applyFilter(p, PhotoFilter::Bw);
  // All three channels should be equal after B/W.
  EXPECT_EQ(p.rgba[0], p.rgba[1]);
  EXPECT_EQ(p.rgba[1], p.rgba[2]);
}

KIMIA_TEST(Photo_HeatShiftsRed) {
  PhotoCapture p;
  p.rgba = {100, 100, 100, 255};
  PhotoCapture before = p;
  applyFilter(p, PhotoFilter::Heat);
  EXPECT(p.rgba[0] > before.rgba[0]);
}

KIMIA_TEST(Photo_CoolBlueShiftsBlue) {
  PhotoCapture p;
  p.rgba = {100, 100, 100, 255};
  PhotoCapture before = p;
  applyFilter(p, PhotoFilter::CoolBlue);
  EXPECT(p.rgba[2] > before.rgba[2]);
}

KIMIA_TEST(Photo_PopArtThreshold) {
  PhotoCapture p;
  p.rgba = {50, 150, 250, 255};
  applyFilter(p, PhotoFilter::PopArt);
  EXPECT_EQ(p.rgba[0], 0);
  EXPECT_EQ(p.rgba[1], 255);
  EXPECT_EQ(p.rgba[2], 255);
}

KIMIA_TEST(Photo_VignetteDarkens) {
  PhotoCapture p;
  p.rgba = {200, 200, 200, 255};
  PhotoCapture before = p;
  applyFilter(p, PhotoFilter::Vignette);
  EXPECT(p.rgba[0] < before.rgba[0]);
}

KIMIA_TEST(Photo_CaptionWithScorerOnly) {
  auto c = makeCaption("Pelezinho", "", 2, 1);
  EXPECT(c.find("Pelezinho") != std::string::npos);
  EXPECT(c.find("2-1") != std::string::npos);
}

KIMIA_TEST(Photo_CaptionWithScorerAndTrick) {
  auto c = makeCaption("Ronaldinho", "Elastico", 3, 2);
  EXPECT(c.find("Ronaldinho") != std::string::npos);
  EXPECT(c.find("Elastico") != std::string::npos);
  EXPECT(c.find("3-2") != std::string::npos);
}

KIMIA_TEST(Photo_CaptionEmpty) {
  auto c = makeCaption("", "", 0, 0);
  EXPECT(!c.empty());
  EXPECT(c.find("0-0") != std::string::npos);
}

KIMIA_TEST(Photo_CaptionScoreFormat) {
  auto c = makeCaption("A", "", 10, 8);
  EXPECT(c.find("10-8") != std::string::npos);
}

KIMIA_TEST(Photo_MultipleFiltersChained) {
  PhotoCapture p;
  p.rgba.resize(8 * 8 * 4);
  for (size_t i = 0; i < p.rgba.size(); i += 4) {
    p.rgba[i + 0] = static_cast<kimia::u8>(i & 0xFF);
    p.rgba[i + 1] = static_cast<kimia::u8>((i + 80) & 0xFF);
    p.rgba[i + 2] = static_cast<kimia::u8>((i + 160) & 0xFF);
    p.rgba[i + 3] = 255;
  }
  applyFilter(p, PhotoFilter::Bw);
  for (size_t i = 0; i < p.rgba.size(); i += 4) {
    EXPECT_EQ(p.rgba[i + 0], p.rgba[i + 1]);
    EXPECT_EQ(p.rgba[i + 1], p.rgba[i + 2]);
  }
  applyFilter(p, PhotoFilter::Sepia);
  // Should still produce valid colors.
  EXPECT(!p.rgba.empty());
}
