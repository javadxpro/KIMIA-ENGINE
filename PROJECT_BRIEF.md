# پرامت کامل پروژه فوتبال خیابونی ایران / Brazil Football Street

> این فایل رو ابتدای هر session جدید به AI بده تا context پروژه رو کامل بفهمه.
> زبان: فارسی برای کاربر. کد: C++17. Engine: Kimia.

---

## ۱) پروژه چیه

**نام رسمی ایران:** فوتبال خیابونی ایران: کوی ابوذر
**نام global برند:** Brazil Football Street
**سبک:** بازی فوتبال خیابونی ۴ نفره با تمپو ایرانی
**Engine:** موتور بازی‌سازی کیمیا (Kimia Engine) — engine سفارشی، C++، static libs
**توزیع:** همه کشورها (در نهایت)
**زمان انتشار:** "وقتی باد GTA 6 بخوابد" (یعنی پس از release رسمی GTA 6 و کاهش هایپ)

---

## ۲) تصمیمات ثابت کاربر (اینا رو هرگز عوض نکن)

1. نام engine: **موتور بازی‌سازی کیمیا** (نه "کیمیا" به تنهایی، نه Kimia Studio، نه هیچ نام دیگه)
2. نام global بازی: **Brazil Football Street**
3. نام ایران بازی: **فوتبال خیابونی**
4. توزیع: **همه کشورها** (در launch همه‌جا یکباره)
5. **DDA ممنوع.** Comeback Burst یک **reward** است نه adjustment:
   - +۴۰٪ سرعت × ۲ ثانیه
   - Cooldown ۶۰ ثانیه
   - فقط trigger می‌شه وقتی بازیکن در حال باخت **≥۲ گل** باشه
   - اگه mastery پایین باشه، boost **هدر می‌ره** (پنالتی skill، نه boost اجباری)
6. **Comeback Burst** نام رسمی نسخه global (نه DDA، نه Momentum، نه هر اسم دیگه)
7. **اولویت اجرا:**
   1. ساخت Android build (CI: `Build Android APK`)
   2. ریختن پایه فوتبال خیابونی در Engine
   3. جزئیات و polish
8. **یک commit + push در پایان هر فاز (نه تیکه‌تیکه).**
9. **شماره‌گذاری نسخه معنایی**، همگام با:
   - `Engine/Core/include/kimia/Version.h`
   - `project(VERSION ...)` در CMakeLists.txt
   - الان هردو روی **v0.30.0**

---

## ۳) ویژگی‌های فنی ثابت

### فیزیک
- **TGS solver** (Time Grouping Strategy)
- **CCD** (Continuous Collision Detection)
- **density mass** — جرم از حجم/چگالی، نه عدد ساده

### AI با signature traits
- **PlayerTraits** struct: ۹ trick خاص + mood
- **۹ trick شامل:**
  1. Bicycle Kick
  2. Rabona
  3. Panenka
  4. Cruyff Turn
  5. Elastico (flip-flap)
  6. Marseille Sliding
  7. Puskas-style Drag
  8. Hocus-Pocus
  9. Fake Step-Over
- **mood** با xorshift PRNG + skill gates
- **EffectiveStats** = traits × mood × composure
- AI loop باید integrate بشه به MatchState per tick

### حالت‌های بازی
- **Street** (پیش‌فرض، کوچه + دیوار بتن + مهتابی زرد)
- **Comedy** (هوا cleared، ball bounce عجیب، هواپیماهای کاغذی)
- **Grass** (چمن، طبیعی، مربی آرام)
- **Speed** (سرعت ×۱.۵، توپ سبک‌تر)

### سیستم ارز ۴ لایه
1. **Coin** (سکه ساده، earned per match)
2. **Token** (از چالش/season)
3. **Gem** (rare, premium cosmetic)
4. **Star** (top tier, earned از master streaks)

### Localization ۳ لایه
- **fa-IR** (فارسی، راست‌چین، فونت فارسی)
- **en-US** (پیش‌فرض)
- **pt-BR** (Brazilian Portuguese)
- همه متن‌های UI + voice line ID ها در جدول locale

---

## ۴) ساختار فعلی Engine

### ماژول‌های اصلی (path در Kimia-Engine)
```
Engine/
├── Core/                    ← kimia::i32, f32, FixedTimeStep, Version
├── Math/                    ← Vec, Mat4
├── Physics/                 ← PhysicsWorld (TGS solver), FixedTimeStep stepping
├── World/                   ← Profile-based game world
├── Scene/                   ← Scene graph
├── Profile/                 ← GameProfile (FIFA-style settings)
├── Graphics/                ← BitmapFont, Image, Mesh, Animator, Audio
├── Renderer/                ← GLFunctions, Shader, GpuMesh, SoftwareRenderer, EGL, D3D11Renderer
├── Assets/                  ← Audio + skeleton
├── Platform/                ← InputState, Window
├── Runtime/                 ← RuntimeLoop
├── EditorUI/                ← 100+ editor panel (PowerPanel, Gizmo, RasterBridge...)
├── StreetSoccer/            ← Match, SkillGatedPower, PlayerTraits, ReplaySystem, ...
├── App/                     ← Application-level stuff
└── Profile/

Tests/src/                   ← 100+ test files (StreetSoccerTests, SkillGatedPowerTests, ...)
Examples/                    ← StreetSoccerDemo, First3DScene, GolfGame, ...
Android/                     ← JNI glue + Android Studio project
Documentation/               ← DESIGN.md and prose
CMakeLists.txt               ← top-level build
Tools/termux-build.sh        ← minimal headless g++ build
```

### Library targets در CMakeLists.txt
- `kimia_core` ← Core (Types, Time, Log, Profiler)
- `kimia_math` ← Vec, Mat4
- `kimia_physics` ← PhysicsWorld (TGS solver)
- `kimia_world` ← Profile-aware World, depends on Profile+Scene+Physics+Assets
- `kimia_scene` ← Scene graph
- `kimia_assets` ← Audio, Skeleton, BitmapFont, Mesh, Image
- `kimia_renderer` ← GLFunctions, SoftwareRenderer, EGL, shaders
- `kimia_platform` ← Window, InputState
- `kimia_runtime` ← Runtime loop
- `kimia_editor_ui` ← editor panels (links to math, world, assets, **renderer**, **street_soccer**)
- `kimia_street_soccer` ← the game logic
- `kimia_app` ← top-level app glue
- `kimia_embedded` ← embedded asset pack
- `kimia_jni` ← Android JNI entry

> **نکته critical:** `kimia_editor_ui` باید به هر دوی `kimia_renderer` و `kimia_street_soccer` link باشه، چون PowerPanel به GLFunctions.h نیاز داره.

### فایل‌های مهم StreetSoccer
- `Engine/StreetSoccer/include/kimia/StreetSoccer.h` ← MatchState + resetMatch + detectGoals
- `Engine/StreetSoccer/include/kimia/SkillGatedPower.h` ← ۵ power enum + SkillMetrics + PowerState + TriggerContext
- `Engine/StreetSoccer/include/kimia/PlayerTraits.h` ← SignatureTrick + PlayerTraits + EffectiveStats
- `Engine/StreetSoccer/src/StreetSoccerPhysics.cpp` ← physics bridge
- `Engine/StreetSoccer/src/StreetAIController.cpp` ← AI controller
- `Engine/StreetSoccer/src/ReplaySystem.cpp` ← replay recording
- `Engine/StreetSoccer/src/HighlightCapture.cpp` ← in-game highlights
- `Engine/StreetSoccer/src/PhotoMode.cpp` ← photo capture

---

## ۵) Build commands

### Headless / Termux / plain Linux (بدون CMake, Gradle, SDL)
```bash
chmod +x Tools/termux-build.sh
./Tools/termux-build.sh
./kimia_street_soccer --frames 600 --auto-kick
```
خروجی نمونه:
```
Street Soccer Demo
Frames: 600 (10.0 simulated seconds)
Teams: Home=4 players, Away=4 players
---
Frame   434 (t=   7.3s): GOAL! Home=0 Away=1
---
Final: Home 0 - Away 1
```

### Desktop CMake (CI also uses this internally for some tests)
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

### Android via Gradle
```bash
cd Android
gradle assembleDebug --stacktrace
# -> app/build/outputs/apk/debug/*.apk
```

---

## ۶) CI در GitHub Actions

**Workflow:** `.github/workflows/android-apk.yml`
**Triggers:** push به `arena/01a0a585-kimia-engine` یا `arena/01a080a4-ai-codespace`

**Steps:**
1. Checkout
2. Set up JDK 17 (setup-java@v4)
3. Install Android SDK (commandlinetools + sdkmanager)
4. **Set up Gradle 8.7 directly** (manual download — cache layer 400 workaround)
5. Build debug APK (gradle assembleDebug --stacktrace)
6. Upload build-log artifact
7. **Post log to PR #6 comment** (only way to read log in sandbox; blob storage blocked)
8. Upload APK

**Permissions required:** `contents: read`, `issues: write`, `pull-requests: write`

---

## ۷) شاخه‌های Git

- `main` ← شاخه اصلی پروژه (متعلق به session/agent دیگه، **دست نزن**)
- `arena/01a080a4-ai-codespace` ← شاخه موازی (پایه `c94ea21`)
- `arena/01a0a585-kimia-engine` ← شاخه **این session** (سر کار هستیم)
  - HEAD = `9b920eb` در زمان نوشتن این سند
  - حاوی: phase-3 full release + Termux build

**قانون مهم:** شاخه `main` و `arena/01a080a4-ai-codespace` مال session های موازی هستن. فقط روی `arena/01a0a585-kimia-engine` کار کن، فقط force push کن اگه ۱۰۰٪ مطمئنی remote مال خودت هست.

---

## ۸) کارهای آینده (roadmap فاز ۴ به بعد)

### کوتاه‌مدت (بعد از این brief)
- [ ] **Power HUD renderer** — نمایش ۵ power + mastery bar + cooldown overlay در حین match روی device
- [ ] **ScriptedSequences** system — replay کامل ۹۰ ثانیه از چند دوربین، برای highlight reel
- [ ] **localization ۳ لایه** — جدول locale + load on-demand، فونت فارسی RTL برای fa-IR
- [ ] **AI loop integration** — PlayerTraits → StreetAIController per-tick behavior، الان trait data هست ولی AI loop integrate نیست
- [ ] **physics bridge به skill traits** — EffectiveStats باید واقعاً بر Brownian force/torque اعمال بشه

### میان‌مدت
- [ ] **TGS solver + CCD + density mass** الان PhysicsWorld basic step داره، باید به TGS/CCD ارتقاء پیدا کنه با استفاده از فرمول Liang Huang
- [ ] **۴ حالت بازی** (Street / Comedy / Grass / Speed) — هر کدوم GameProfile override داشته باشه
- [ ] **سیستم ارز ۴ لایه** — Coin/Token/Gem/Star inventory service + UI panel

### بلند-مدت
- [ ] **launch trailer** و storyboard
- [ ] **Brazilian Portuguese voice acting** (VoiceOver Pipeline)
- [ ] **multiplayer networking** (به‌علاوه ۴-player street) — شاید در فاز ۵
- [ ] **GTA 6 launch window watch** — release رصد، هایپو کاهش، launch trailer timing

---

## ۹) لحن و ارتباط با کاربر

- **فارسی حرف بزن.** کد انگلیسی. commit message انگلیسی.
- **کمتر حرف، بیشتر کد** — کاربر گفته "انقد حرف نزن کدها رو بنویس"
- **بدون تأیید بپرس تا آخرین حد** — کاربر گفته "دارم میرم جایی"؛ کار رو ادامه بده.
- **touch-only / incremental** — هر فاز، یه چیز کوچک اضافه کن، push کن، ادامه.
- **CI حفظ شود** — workflow `Build Android APK` نباید حذف بشه.
- **یک commit + push در پایان هر فاز** — تیکه‌تیکه push نکن.
- **وقتی سؤال خارج از scope پرسیده شد**، با `ask_user` چند گزینه بده، نه متن باز.

---

## ۱۰) اشتباهات گذشته (نکن!)

1. **force push بدون چک remote contents.** یه بار همه commit های فاز ۳ با force پاک شد. الان fix شده ولی replay می‌شه اگه بی‌دقتی کنی.
2. **`f-string` با backslash در `python3 -c` inline** → SyntaxError. راه: استفاده از `print(...) + keys[a] + ...` یا اینکه python script رو توی فایل بنویس.
3. **`gh auth token expiry.** session طولانی → GH_TOKEN منقضی می‌شه. اگه `Bad credentials` دید، از کاربر PAT بگیر.
4. **Azure blob storage از sandbox بلاک** (`productionresultssa*.blob.core.windows.net: SSL_ERROR_SYSCALL`) → نمی‌تونی artifact مستقیم دانلود کنی. راه حل: workflow با curl API comment بذاره تو PR.
5. **استنباط "باگ در کد" قبل از چک کردن include paths** غلطه. اول `g++ -fsyntax-only -I ...` بزن، اگه ۱۰۰% درست نشد سراغ CMake linking برو.
6. **`EditorUI.cpp → GLFunctions.h` file not found** وقتی `kimia_renderer` link نباشه. این ترکیب حیاتیه.
7. **duplicate symbol `kimia::ui::takeDrawCmds()`** — قبلاً هر دو EditorUI.cpp و RasterBridge.cpp تعریفش کرده بودن. حل: فقط EditorUI.cpp define کنه.
8. **Gradle cache 400** → cache service responds with 400. حل: Gradle 8.7 مستقیم از services.gradle.org دانلود کن، نه از `gradle/actions/setup-gradle@v3`.

---

## ۱۱) شروع کار session جدید

اگه تازه وارد این پروژه شدی:
1. اول وضعیت شاخه‌ها رو بپرس:
   ```bash
   git log --oneline origin/arena/01a0a585-kimia-engine -10
   ```
2. سپس CI آخرین run:
   ```bash
   gh run list --workflow=android-apk.yml --branch=arena/01a0a585-kimia-engine --limit=3
   ```
3. سپس engine build test محلی:
   ```bash
   ./Tools/termux-build.sh && ./kimia_street_soccer --frames 60
   ```
4. سپس اولین فاز roadmap رو بگیر (Power HUD یا ScriptedSequences).

**شروع قدرتمند:** Phase 4 = Power HUD renderer → ترکیب PowerPanel + renderer + per-tick overlay.

---

این brief رو اگه می‌خوای به یه AI دیگه بدی، کافیه این فایل رو لینک کنی یا متنش رو در پرامت اول session کپی کن.
