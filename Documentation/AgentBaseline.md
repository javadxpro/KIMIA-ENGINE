# Agent Baseline — بررسی عمیق KIMIA-ENGINE

تاریخ ثبت: 2026-09-24 · ثبت‌کننده: Agent Mode (Arena)
این سند خروجی «فاز ۰» مأموریت بررسی است: فقط faktaهای قابل‌اندازه‌گیری، بدون refactor.

## 1. هویت repository

| مورد | مقدار |
| --- | --- |
| branch فعال | `arena/01a0c3a7-kimia-engine` |
| tip هنگام شروع بررسی | `10b9ffe` (پس از اصلاحات: `758f0b1`) |
| base مأموریت | `6e60a07` (= `feature/engine-10-of-10` = `arena/01a080a4-ai-codespace` روی remote) |
| commitها base..HEAD | 33 (هنگام شروع بررسی) |
| diff base..HEAD | 82 فایل، ‎+10972 / −2492 خط |
| PR #8 | **CLOSED**؛ head = `6e60a07`، base = `main`؛ بدون review و بدون comment |
| branch واقعی کار | شاخهٔ نشست (`arena/01a0c3a7-kimia-engine`)؛ PR بازِ متناظر وجود ندارد |

## 2. محیط اجرا

| ابزار | وضعیت |
| --- | --- |
| g++ | 12.2.0 (Debian) |
| CMake | 4.4.3 (pip user install؛ در image پایه نبود) |
| Ninja | 1.13.2 |
| CPU / OS | 2 هسته · Linux x86_64 |
| SDL2 / GL / EGL / GLES3 | هیچ‌کدام (هدر و pkg-config موجود نیست) |
| Java / Gradle / NDK | موجود نیست |
| Emscripten | موجود نیست |
| clang-tidy / cppcheck | موجود نیست |

نتیجه: Android/WebGL/Windows در این محیط **فقط از راه CI** راستی‌آزمایی می‌شوند؛ اجرای APK روی دستگاه هنا ممکن نیست («این مورد تأیید اجرایی نشده است»).

## 3. وضعیت build/test اولیه (لحظهٔ شروع بررسی)

- build Release با `-Werror` + تست‌ها + ابزارها: **سبز** (build-review، Ninja، ~99s).
- `ctest --test-dir build-review`: **FAIL** — `sceneio_shipped_worlds_roundtrip_byte_identical`
  روی `Worlds/street_match.kimia` (فایل غیرکاننیکال نسبت به نویسنده). توجه: اجرای مستقیم
  باینری قدیمی پیش‌تر «563/563» چاپ کرده بود — **سبز کاذب به دلیل باینری stale**؛ gate واقعی
  (ctest/CI) آن را گرفت. اصلاح: کاننیکال‌سازی فایل با خود `WorldIO::save` در `8570431`.
- CI روی `10b9ffe`: jobهای Linux و هر دو Sanitizer **failure** (همان تست)، Windows/WASM success.
  روی `8570431`: هر سه job **success**.
- Sanitizer مأموریت (Debug + ASan/UBSan + `-Werror`): build سبز، ctest **100%** (102s).
- `kimia_world --version` → `KIMIA 0.29.1` (rc=0)؛ `--help` rc=0.

## 4. blockerهای یافته‌شده و اصلاح‌شده حین بررسی

1. **Android launch blocker** — `NativeEngine.java:5` بارگذاری `kimia_engine` ولی CMake بدون
   `OUTPUT_NAME` → `libkimia_jni.so`. APK build می‌شد ولی هرگز اجرا نمی‌شد (CI فقط build).
   اصلاح: `set_target_properties(kimia_jni PROPERTIES OUTPUT_NAME "kimia_engine")` + دو تست
   قرارداد در `Tests/src/AndroidConfigTests.cpp` (هر دو سمت قرارداد پین شده؛ گاز گرفتن تست با
   probe منفی اثبات شد). commit `758f0b1`.
2. **world غیزکاننیکال** — توضیح بالا؛ commit `8570431`.

## 5. targetهای CMake (واقعی، از CMakeLists.txt)

کتابخانه‌ها: `kimia_core, kimia_runtime, kimia_math(INTERFACE), kimia_scene, kimia_physics,
kimia_profile, kimia_assets, kimia_renderer, kimia_platform, kimia_app, kimia_world, kimia_view,
kimia_golf, kimia_raytracer` + کتابخانه‌های vendored (`kimia_ufbx, kimia_stb_*, kimia_dr`).
اجراشدنی‌ها: `kimia_tests, kimia_hello, kimia_first3d, kimia_remote, kimia_world_app,
kimia_golf_app` + ابزارها (`kimia_bench_physics, kimia_assets_cli, kimia_asset_gen,
kimia_raytrace_cli`) + `kimia_jni` (فقط با `KIMIA_BUILD_ANDROID_JNI=ON`).

جهت وابستگی مشاهده‌شده: `view → world/renderer/platform`؛ `world → profile/scene/physics/assets`
(بدون renderer)؛ `raytracer → world`؛ `renderer → assets/math`. هیچ include از World به Renderer
وجود ندارد و هیچ رشتهٔ gameplay (`ball/goal/tackle`) در `Engine/Renderer/src` نیست.

## 6. workflowها

فعال (gate): `ci.yml` (Linux GCC -Werror + ctest + banner + app smoke؛ Sanitizers ADDRESS_UNDEFINED؛
Sanitizers THREAD؛ Windows MSVC smoke؛ WebAssembly smoke)، `android-apk.yml`، `windows-exe.yml`.
در base فقط نسخه‌های `.disabled` وجود داشت؛ فعال‌سازی کار همین شاخه است. نسخه‌های `.disabled`
به‌عنوان تاریخچه مانده‌اند و CI فعال حساب نمی‌شوند.

## 7. ادعاهای مستندات که در کد تأیید نشدند

- «کپسول kinematic» در `Documentation/Physics.md:29` در حالی که `CharacterBody` در
  `Physics.h:105` جعبهٔ AABB با `halfExtents` است (proxy جعبه، نه کپسول واقعی).
- فاز ۵ (ماشین حالت انیمیشن از velocity، foot IK، blend شتاب/کاهش) وجود ندارد؛ آنچه هست پخش clip
  روی اسکلت است (`playClip/startClip/posedMesh` در `Engine/World/src/Animation.cpp`).
  `Documentation/Animation.md` خودش ادعای state machine ندارد → سند صادق است.
- فاز ۷ (CameraTrack/Replay/…) هیچ‌جا در کد نیست؛ ROADMAP صریح می‌گوید «با فاز ۷ می‌آید» → صادق.
- ۹ نقش AI در brief؛ در کد ۵ نقش (`Keeper/Attack/Defend/Support/Idle`، `Ai.cpp:80`).
- صفات seed-محور (Vision/Reflexes/…) پیاده‌سازی نشده‌اند؛ فقط `aiSkill` سراسری.

## 8. خطرهای اولیه مشاهده‌شده

- باینری stale → سبز کاذب محلی (درس فرآیندی: قبل از ادعا، rebuild + ctest).
- CI فقط build برای Android/WebGL/Windows → هر قرارداد runtime-only باید تست متنی/دستگاهی داشته باشد.
- تعادل بازی: مسابقهٔ ۳ دقیقه‌ای بدون انسان ۱–۱۱ و نمونهٔ ۷ ثانیه‌ای ۰–۱۷ → keeper ضعیف.
- `jni_glue.cpp` (۱۲۷۰ خط) و `World.h` (۱۳۲۸ خط) هنوز بزرگ‌اند؛ `WorldEditorApp.cpp` به ۶۷۸ خط کاهش یافته.
- یک symbol JNI اضافی بدون declaration در Java: `nativeEditGetColor` (مرده، بی‌خطر).

## 9. فهرست کارهای پیشنهادی (اولویت‌بندی‌شده)

1. تعادل keeper/دفاع (بازی قابل تماشا) — high، gameplay.
2. فاز ۵: ماشین حالت گیت از velocity واقعی + foot IK ساده + blend شتاب — high.
3. تست دستگاه/امولاتور Android برای launch واقعی (یا emulator در CI) — high.
4. fuzzing برای SceneIO/WorldIO/profile parser — medium (فاز ۱۰ brief).
5. golden-image برای software renderer به‌صورت فایل (فعلاً تست پیکسل دقیق موجود است) — medium.
6. تقسیم `jni_glue.cpp` به lifecycle/input/editor — medium.
7. replay deterministic (input+events+seed) — medium/بالا برای فاز ۷.
8. حذف symbol مردهٔ `nativeEditGetColor` یا افزودن declaration آن — low.
9. اصطلاح‌سازیDocs: «کپسول» → «جعبهٔ kinematic» تا کد کپسول واقعی نیامده — low.
