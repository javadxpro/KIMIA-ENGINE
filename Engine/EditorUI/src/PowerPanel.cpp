#include <kimia/PowerPanel.h>
#include <kimia/EditorUI.h>
#include <kimia/Widget.h>
#include <kimia/Theme.h>

namespace kimia::ui {

namespace {
const char* descShort(kimia::street::StreetPower p) {
  using namespace kimia::street;
  switch (p) {
    case StreetPower::ComebackBurst:   return "Speed x1.4 / 2s when losing >=2";
    case StreetPower::LastStand:       return "Shot x1.5 / last 30% + losing";
    case StreetPower::CrowdBoost:      return "Stamina x0.5 / tied / 5s";
    case StreetPower::StreetSense:     return "Vision x2 / opp near goal / 1.5s";
    case StreetPower::DoubleOrNothing: return "Shot x1.3 after 3 passes";
  }
  return "?";
}
f32 masteryOf(const kimia::street::SkillMetrics& m,
              kimia::street::StreetPower p) {
  using namespace kimia::street;
  switch (p) {
    case StreetPower::ComebackBurst:   return m.masteryComeback;
    case StreetPower::LastStand:       return m.masteryLastStand;
    case StreetPower::CrowdBoost:      return m.masteryCrowd;
    case StreetPower::StreetSense:     return m.masteryStreetSense;
    case StreetPower::DoubleOrNothing: return m.masteryDouble;
  }
  return 0.0f;
}
}

void drawPowerPanel(const Rect& rect,
                    const kimia::street::PowerState& state,
                    i32 selectedIndex) {
  using namespace theme;
  using namespace kimia::street;
  drawRect(rect, kPanel, 0.0f);
  drawText("Street Powers", rect.x + 6.0f, rect.y + 4.0f, 1, kText);

  constexpr f32 rowH = 60.0f;
  const StreetPower all[] = {
    StreetPower::ComebackBurst, StreetPower::LastStand,
    StreetPower::CrowdBoost,    StreetPower::StreetSense,
    StreetPower::DoubleOrNothing
  };

  f32 y = rect.y + 18.0f;
  for (int i = 0; i < 5; ++i) {
    if (y + rowH > rect.y + rect.h) break;
    const StreetPower p = all[i];
    const bool sel = (i == selectedIndex);
    const bool active = isPowerActive(state, p);
    const bool cd = isPowerOnCooldown(state, p);
    const f32 mastery = masteryOf(state.metrics, p);

    drawRect({rect.x + 4.0f, y, rect.w - 8.0f, rowH - 4.0f},
             sel ? kAccentDim : kPanelAlt, 1.0f);
    drawText(powerName(p),
             rect.x + 8.0f, y + 4.0f, 1,
             active ? kAccentHot : (sel ? kAccent : kText));
    drawText(descShort(p),
             rect.x + 8.0f, y + 18.0f, 1, kTextMuted);

    // Mastery bar.
    const f32 barX = rect.x + 8.0f;
    const f32 barW = rect.w - 80.0f;
    const f32 barY = y + rowH - 14.0f;
    drawRect({barX, barY, barW, 4.0f}, kPanel, 0.0f);
    drawRect({barX, barY, barW * mastery, 4.0f},
             mastery > 0.5f ? kSuccess : kAccent, 0.0f);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d%%",
                  static_cast<int>(mastery * 100.0f));
    drawText(buf, barX + barW + 4.0f, barY - 2.0f, 1, kText);

    // Status badge.
    if (active) {
      std::snprintf(buf, sizeof(buf), "%.1fs",
                    static_cast<double>(powerTimeRemaining(state, p)));
      drawText(buf, rect.x + rect.w - 36.0f, y + 4.0f,
               1, kAccentHot);
    } else if (cd) {
      std::snprintf(buf, sizeof(buf), "%.0fs",
                    static_cast<double>(powerCooldownRemaining(state, p)));
      drawText(buf, rect.x + rect.w - 36.0f, y + 4.0f, 1, kTextMuted);
    } else {
      drawText("READY", rect.x + rect.w - 42.0f, y + 4.0f, 1, kSuccess);
    }

    y += rowH;
  }
}

}
