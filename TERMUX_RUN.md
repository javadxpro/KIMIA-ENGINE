# فوتبال خیابونی ایران روی Termux

## راه‌اندازی در Poco X3 Pro یا هر گوشی اندروید

۱. **Termux را نصب کن** از [F-Droid](https://f-droid.org/en/packages/com.termux/) (نه Google Play، آپدیت‌هاش قدیمیه).

۲. **محیط توسعه را فعال کن:**
```bash
pkg update && pkg upgrade
pkg install git clang make
```

۳. **کلون کن:**
```bash
cd ~
git clone https://github.com/javadxpro/KIMIA-ENGINE.git
cd KIMIA-ENGINE
git checkout arena/01a0a585-kimia-engine
```

۴. **بیلد:**
```bash
chmod +x Tools/termux-build.sh
./Tools/termux-build.sh
# تولید: ./kimia_street_soccer
```

۵. **اجرا:**
```bash
# حالت پیش‌فرض — ۱۰ ثانیه شبیه‌سازی AI vs AI
./kimia_street_soccer

# با داستان کامل (Termux color output)
./kimia_street_soccer --story

# تست comeback burst (force away team to score 2 goals)
./kimia_street_soccer --story --force-away-2 2

# ASCII pitch view
./kimia_street_soccer --pitch
```

## چی می‌بینی؟

```
Street Soccer Demo
Frames: 600 (10.0 simulated seconds)
Teams: Home=4 players, Away=4 players
Pitch: 28.0m x 16.0m
Mode: AI +story
---
Frame   434 (t=   7.3s): GOAL! Home=0 Away=1
---
Final: Home 0 - Away 1

[سپس story timeline رنگی]

▶ [t=  0.0s] INTRO
  Afternoon kick-off on Koye-Abouzar street
  ظهر توي کوچه‌ي ابوذر، صدای توپ پلاستيکي مياد

▶ [t=  1.0s] TRICK
  Mohsen pulls off a Stepover out of nowhere
  محسن يهوويه استپ‌اور ميزنه و دو نفر گيج ميشن

▶ [t=  4.5s] HALF TIME
  ...

▶ [t=  7.3s] GOAL
  Away counter-attack — home 0 - away 1
  ضد‌حمله‌ي مهمون! خونه 0 - مهمون 1

▶ [t=  9.5s] FULL TIME
  ...

══════════════════════════════════════════
FULL TIME
══════════════════════════════════════════

  Home 0  —  1 Away
  ⚪ No Comeback Burst — game stayed close. Skill > RNG.
```

## کنترل‌ها (آینده)

فاز بعد Interactive Mode روی Termux:
```bash
./kimia_street_soccer --interactive
# Keys:
#   q w e r t y u — players 1-4 position
#   a s d f g h — players 5-8 position
#   j k l — shoot pass tackle
#   space — kickoff
```

## نکات

- Termux باید روی storage permission بده اگه می‌خوای replay ها save شون (`termux-setup-storage`)
- Render: ANSI colors — اگه رنگ ندیدی، `pkg install ncurses-utils` شاید کمک کنه
- ۱۰ ثانیه شبیه‌سازی در CPU معمولی ≈ ۰.۱ ثانیه واقعی
- AI هر team ۴ player signature traits مختلف دارن (9 trick نوع، xorshift mood)

## محدودیت‌ها

- بدون graphics — فقط ASCII pitch + ANSI text. برای graphics باید Android APK.
- Replay save کار نمی‌کنه فعلاً (روی Android کار می‌کنه).
- AI home vs away بصورت تصادفی، هنوز balance کامل نیست — Home 0-1 اغلب.

## گزارش مشکل

اگه build نشد یا termux-specific crash:
- `g++ --version` بفرست
- `pkg list-installed | grep clang` بفرست
- `uname -a` بفرست (اندروید kernel ورژن)
