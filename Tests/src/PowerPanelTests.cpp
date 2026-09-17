#include <kimia_test.h>
#include <kimia/PowerPanel.h>
#include <kimia/SkillGatedPower.h>

KIMIA_TEST(PowerPanel_DrawDefaultDoesNotCrash) {
  auto state = kimia::street::makeDefaultPowerState();
  kimia::ui::drawPowerPanel({0, 0, 320, 360}, state, -1);
}

KIMIA_TEST(PowerPanel_DrawWithActivePower) {
  auto state = kimia::street::makeDefaultPowerState();
  kimia::street::TriggerContext ctx; ctx.scoreDiff = -2;
  tryActivatePower(state, kimia::street::StreetPower::ComebackBurst, ctx);
  kimia::ui::drawPowerPanel({0, 0, 320, 360}, state, 0);
}

KIMIA_TEST(PowerPanel_DrawWithMastery) {
  auto state = kimia::street::makeDefaultPowerState();
  for (int i = 0; i < 8; ++i) {
    recordPowerOutcome(state, kimia::street::StreetPower::ComebackBurst, true);
  }
  for (int i = 0; i < 3; ++i) {
    recordPowerOutcome(state, kimia::street::StreetPower::LastStand, true);
  }
  kimia::ui::drawPowerPanel({0, 0, 320, 360}, state, 2);
}

KIMIA_TEST(PowerPanel_DrawAtPhonePortrait) {
  auto state = kimia::street::makeDefaultPowerState();
  kimia::street::TriggerContext ctx;
  ctx.tiedScore = true;
  tryActivatePower(state, kimia::street::StreetPower::CrowdBoost, ctx);
  ctx.consecutivePasses = 3;
  tryActivatePower(state, kimia::street::StreetPower::DoubleOrNothing, ctx);
  kimia::ui::drawPowerPanel({0, 0, 240, 320}, state, 1);
}

KIMIA_TEST(PowerPanel_DrawAtTabletLandscape) {
  auto state = kimia::street::makeDefaultPowerState();
  for (int p = 0; p < 5; ++p) {
    for (int i = 0; i < 5; ++i) {
      recordPowerOutcome(state,
        static_cast<kimia::street::StreetPower>(p), i % 2 == 0);
    }
  }
  kimia::ui::drawPowerPanel({0, 0, 480, 320}, state, 0);
}
