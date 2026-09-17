#pragma once
#include "EditorUI.h"
#include <kimia/SkillGatedPower.h>

namespace kimia::ui {

void drawPowerPanel(const Rect& rect,
                    const kimia::street::PowerState& state,
                    i32 selectedIndex);

}
