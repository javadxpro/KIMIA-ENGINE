// Real impl of label functions.
#include <kimia/RandomEvents.h>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace kimia::street {

const char* eventLabel(RandomEventKind k) {
  switch (k) {
    case RandomEventKind::StrayDog:       return "STRAY DOG";
    case RandomEventKind::BasketDrop:     return "BASKET DROP";
    case RandomEventKind::Blackout:       return "BLACKOUT";
    case RandomEventKind::KidJoins:       return "KID JOINS";
    case RandomEventKind::DustStorm:      return "DUST STORM";
    case RandomEventKind::EveningPrayer:  return "EVENING PRAYER";
    case RandomEventKind::IceCreamTruck:  return "ICE CREAM TRUCK";
    case RandomEventKind::NewspaperBlows: return "NEWSPAPER";
    case RandomEventKind::PigeonHerd:     return "PIGEONS";
    case RandomEventKind::TabbyCat:       return "TABBY CAT";
  }
  return "?";
}

const char* eventLabelFa(RandomEventKind k) {
  switch (k) {
    case RandomEventKind::StrayDog:       return "سگ ولگرد";
    case RandomEventKind::BasketDrop:     return "سبد چرخ افتاد";
    case RandomEventKind::Blackout:       return "برق رفت";
    case RandomEventKind::KidJoins:       return "بچه محل اومد";
    case RandomEventKind::DustStorm:      return "گرد و خاک";
    case RandomEventKind::EveningPrayer:  return "اذان مغرب";
    case RandomEventKind::IceCreamTruck:  return "ماشین بستنی";
    case RandomEventKind::NewspaperBlows: return "روزنامه پرید";
    case RandomEventKind::PigeonHerd:     return "کبوترها";
    case RandomEventKind::TabbyCat:       return "گربه محل";
  }
  return "?";
}

namespace {
RandomEvent make(RandomEventKind k, const std::string& fa,
                 const std::string& en, f32 dur, f32 fric = 0,
                 f32 grav = 0, f32 fov = 0, bool pause = false,
                 i32 exH = 0, i32 exA = 0) {
  RandomEvent r;
  r.kind = k;
  r.headlineFa  = fa;
  r.headlineEn  = en;
  r.durationSeconds = dur;
  r.frictionDelta   = fric;
  r.gravityDelta    = grav;
  r.pitchFovDelta   = fov;
  r.pauseMatch      = pause;
  r.extraHomePlayers = exH;
  r.extraAwayPlayers = exA;
  return r;
}
}  // namespace

std::vector<RandomEvent> generateRandomEvents(u32 seed, f32 matchSeconds) {
  std::vector<RandomEvent> out;
  u32 state = seed ? seed : 0x9E3779B9u;
  auto nextRand = [&]() -> u32 {
    state = state * 1664525u + 1013904223u;
    return state;
  };

  const u32 nEvents = 2 + (nextRand() % 3);
  f32 lastTime = 0;
  for (u32 i = 0; i < nEvents; ++i) {
    f32 minOffset = 30.0f;
    f32 maxOffset = std::max(60.0f, (matchSeconds - lastTime) * 0.45f);
    f32 offset = minOffset + (nextRand() % static_cast<u32>(maxOffset - minOffset));
    f32 t = lastTime + offset;
    if (t >= matchSeconds) t = matchSeconds - 5.0f;
    if (t < lastTime + 5.0f) t = lastTime + 5.0f;

    RandomEventKind kind = static_cast<RandomEventKind>(nextRand() % 10);
    RandomEvent ev;
    switch (kind) {
      case RandomEventKind::StrayDog:
        ev = make(kind, "یه سگ ولگرد دوید تو زمین",
                  "A stray dog charges across the pitch", 6.0f); break;
      case RandomEventKind::BasketDrop:
        ev = make(kind, "سبد چرخ از بالا افتاد روی بازیکن شماره ۴ خونه",
                  "Bicycle basket drops on Home #4", 5.0f); break;
      case RandomEventKind::Blackout:
        ev = make(kind, "برق رفت! تاریک شد",
                  "Blackout. Pitch dark.", 4.0f, 0, 0, -15.0f); break;
      case RandomEventKind::KidJoins:
        ev = make(kind, "یه بچه محل اومد تو، یه بازیکن جدید به خونه",
                  "Local kid joins in; home team +1 player", 20.0f, 0, 0, 0, false, 1); break;
      case RandomEventKind::DustStorm:
        ev = make(kind, "باد گرد و خاک بلند کرد، اصطکاک بالا رفت",
                  "Dust storm — pitch friction up", 8.0f, 0.30f); break;
      case RandomEventKind::EveningPrayer:
        ev = make(kind, "اذان مغرب — بازی ۵ ثانیه متوقف",
                  "Evening prayer — 5 second pause", 5.0f, 0, 0, 0, true); break;
      case RandomEventKind::IceCreamTruck:
        ev = make(kind, "ماشین بستنی اومد، بچه‌ها یه لحظه گوش دادن",
                  "Ice cream truck jingle — players distracted", 4.0f); break;
      case RandomEventKind::NewspaperBlows:
        ev = make(kind, "یه روزنامه از کوچه پرید تو زمین",
                  "A newspaper from the alley blows onto the pitch", 3.0f); break;
      case RandomEventKind::PigeonHerd:
        ev = make(kind, "یه دسته کبوتر بلند شدن، توپ ترسید",
                  "Pigeon herd scatters — ball startled", 5.0f); break;
      case RandomEventKind::TabbyCat:
        ev = make(kind, "یه گربه محل اومد، توپ رو دنبال کرد",
                  "Tabby cat chases the ball", 7.0f); break;
    }
    out.push_back(ev);
    lastTime = t;
  }
  return out;
}

}  // namespace kimia::street
