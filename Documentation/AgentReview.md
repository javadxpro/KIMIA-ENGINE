# گزارش نهایی بررسی KIMIA

تاریخ: 2026-09-24 · شاخه: `arena/01a0c3a7-kimia-engine` · tip نهایی: `0e46a83` · base: `6e60a07`
روش: هر ادعا با مسیر فایل/تابع/commit یا اجرای واقعی پشتیبانی شده؛ هر آنچه اجرا نشد صریحاً نوشته شده است.

## 1. پاسخ کوتاه

موتور واقعاً بهتر شده است، نه فقط پر حجیم‌تر: ۳۶ commit نسبت به base، ۵۶۵ تست رفتارمحور (از ~۴۷۵)،
چهار workflow فعال که همگی روی tip نهایی سبزند، فیزیک با broad phase واقعی (۶۴× سریع‌تر در ۱۰۰ جسم)،
CCD تست‌شده، ماده‌های زمین، و AI که پاس می‌دهد و تصمیمش را با امتیاز توضیح می‌دهد. دو blocker واقعی
حین بررسی پیدا و با تست_regression اصلاح شد (نام کتابخانهٔ Android که APK را غیرقابل اجرا می‌کرد، و
world غیزکاننیکال که ctest/CI آن را گرفت). آنچه هنوز نیست equally روشن است: انیمیشن velocity-محور و
foot IK وجود ندارد، replay/سینماتیک اصلاً نیست، Android هرگز روی دستگاه اجرا نشده، و تعادل بازی
(۰–۱۷ در نمونهٔ مشاهده‌شده) قابل تماشا نیست. پروژه prototype ضعیف نیست؛ یک foundation خوب است که
لایه‌های بالایی‌اش (انیمیشن، ری‌پلی، محتوا) هنوز خالی‌اند.

## 2. نمره

**نمرهٔ کلی: 6.5 / 10**

| بخش | نمره | دلیل یک‌خطی |
| --- | --- | --- |
| معماری | 7.5 | لایه‌ها یک‌طرفه‌اند (view→world→scene/physics؛ world بدون renderer)؛ World.h هنوز ۱۳۲۸ خط و jni_glue ۱۲۷۰ خط |
| Physics | 7.0 | broad phase + CCD + ماده‌ها + determinism تست‌شده؛ ولی بدون چرخش/اینرسی/COM/sleeping و کاراکتر AABB است نه کپسول |
| Animation | 3.0 | پخش clip روی اسکلت و retarget هست؛ ماشین حالت، foot IK و blend شتاب نیست |
| AI | 5.5 | تصمیم امتیازدهی‌شدهٔ خالص + پاس leadشده + دیباگ‌endpoint؛ ولی ۵ نقش از ۹، بدون صفات seed، تعادل شکسته |
| Rendering | 6.5 | رستر نرم‌افزاری با تست پیکسل دقیق و ریاضی PBR مرجع؛ بک‌اندها در CI کامپایل‌اند؛ بدون golden-image فایل و بدون اجرای GPU هنا |
| Android | 4.5 | APK در CI بیلد می‌شود و قرارداد نام/سمبل حالا تست دارد؛ ولی هرگز اجرا نشده (پیش از اصلاح، اصلاً قابل اجرا نبود) |
| Editor | 7.5 | ۷۶ endpoint با تست و حالت publish قفل؛ بدون undo/redo و چندانتخاب |
| Build/CI | 8.5 | چهار gate فعال (Linux/-Werror/ctest، دو sanitizer، Windows smoke، WASM smoke) + APK + EXE؛ خطا را واقعاً گرفت |
| Tests/QA | 7.5 | ۵۶۵ تست رفتارمحور + probe منفی برای اثبات گاز گرفتن؛ بدون fuzz و بدون تست دستگاه |
| Documentation | 7.5 | ROADMAP صادقانه «نیامده» را جدا می‌کند؛ یک ناسازگاری اصطلاح (کپسول) پیدا و ثبت شد |
| Security | 7.0 | bind پیش‌فرض loopback + auth اجباری برای non-loopback + سقف آپلود ۳۲MB + race تاریخی رفع‌شده؛ بدون fuzz، توکن در URL |
| Performance | 6.5 | bench واقعی: ۱۰۰۰ جسم ۲٫۰–۶٫۲ ms/step؛ رستر نرم‌افزاری روی CPU ضعیف کند است؛ پروفایلر فریم نیست |
| Gameplay readiness | 5.0 | مسابقه اجرا می‌شود و گل می‌خورد؛ بدون انیمیشن/ری‌پلی/pause درون‌بازی و با تعادل شکسته |

## 3. طبقه‌بندی پروژه

**foundation خوب برای موتور** — لایه‌های پایین (build/CI/فیزیک/scene/editor) قابل اتکا و تست‌شده‌اند؛
لایه‌های بالای گیم‌پلی (انیمیشن، ری‌پلی، سینماتیک، محتوا) هنوز ساخته نشده‌اند.

## 4. چیزهایی که واقعاً بهتر شده‌اند (کد + تست + CI)

- CI از «همه‌چیز .disabled» به چهار gate فعال رسید؛ commitهای `fd81c59`, `a8a3456`, `635b500`.
- broad phase sweep-and-prune: بنچ این بررسی ۱۰۰ جسم 0.086 ms در برابر 5.532 ms مسیر all-pairs (64.6×)؛ `d89325f`.
- CCD توپ: شوت ۶۰ m/s از تیرک نمی‌گذرد؛ ۶ تست + تست دنیا؛ `cd61d1e`.
- ماده‌های زمین (۷ ماده، grip/restitution) با serialize و endpoint؛ `212c04b`.
- کنترلگر کاراکتر: step offset و رفع قفل واقعی بازی؛ `3db0e82`.
- AI: تصمیم shoot/pass/carry امتیازدهی‌شده و خالص + رویدادهای لمسی + `/api/ai`؛ `1316481`.
- World از یک فایل غول به ۱۶ فایل ماژولی تقسیم شد بدون تغییر رفتار؛ `185c9d7..eec53b5`.
- AssetManager: یک parse per file، بدون بارگذاری در hot path؛ `0d3d671`, `a33c95e`.
- قرارداد round-trip دنیاها حالا شامل world جدید هم می‌شود و نویسنده تنها منبع حقیقت است؛ `8570431`.
- قرارداد نام JNI/Java پین شد و soname درست شد؛ `758f0b1`.

## 5. ادعاهای تأییدنشده

- «کپسول kinematic» (Documentation/Physics.md:29) ← کد: `CharacterBody::halfExtents` جعبه است (Physics.h:105).
- اجرای APK روی دستگاه، اجرای exe روی Windows، اجرای WebGL در مرورگر: **تأیید اجرایی نشده** (فقط build در CI).
- فاز ۵ و ۷ brief (حالت‌های انیمیشن، foot IK، CameraTrack/Replay): در کد نیستند؛ مستندات خودِ repo هم ادعا نمی‌کند (صادق).
- ۹ نقش AI و صفات seed: فقط در brief/ROADMAP؛ کد ۵ نقش و یک `aiSkill` سراسری دارد.
- clang-tidy/cppcheck/fuzzing: ابزار در محیط نیست → اجرا نشده.

## 6. blockerهای merge

هیچ blocker بازِ شناخته‌شده باقی نمانده؛ دو مورد یافته‌شده حین بررسی اصلاح و تست شدند:

1. severity: blocker · فایل: `Android/.../NativeEngine.java:5` + `CMakeLists.txt:557` · دلیل: loadLibrary("kimia_engine") در برابر libkimia_jni.so · اثر: UnsatisfiedLinkError در هر launch، در حالی که CI سبز می‌ماند · راه‌حل اعمال‌شده: OUTPUT_NAME + دو تست قرارداد (`758f0b1`).
2. severity: blocker · فایل: `Worlds/street_match.kimia` · دلیل: فایل غیزکاننیکال نسبت به نویسنده → شکست ctest/CI · اثر: gate واقعی قرمز · راه‌حل اعمال‌شده: کاننیکال‌سازی با خود writer (`8570431`).

## 7. مشکلات مهم بعد از merge (high/medium)

- تعادل keeper/دفاع (high): نمونهٔ مشاهده‌شده ۰–۱۷؛ keeper توپ را می‌خواند ولی به‌ندرت می‌گیرد.
- فاز ۵ انیمیشن (high): بدون آن، بازی «فوتبال» دیده نمی‌شود حتی وقتی قوانینش درست‌اند.
- اجرای واقعی Android (high): emulator/device smoke در CI یا چک‌لیست دستی.
- fuzz برای SceneIO/WorldIO/profile (medium).
- تقسیم jni_glue.cpp و World.h (medium).
- symbol مردهٔ `nativeEditGetColor` در glue بدون declaration در Java (low).
- توکن auth در query string URL لوگ می‌شود؛ header-only کردنش (medium، امنیتی).

## 8. وضعیت قابلیت‌های فوتبال خیابانی

| قابلیت | وضعیت | شواهد | کار باقی‌مانده |
| --- | --- | --- | --- |
| 3v3 / 5v5 | پیاده‌سازی‌شده و تست‌شده | `# profile team N` + مسابقهٔ زندهٔ 5v5 | تست صریح 3v3 |
| بازیکن قابل کنترل | پیاده‌سازی‌شده و تست‌شده | input API + WorldTests | — |
| پاس / شوت / دریبل / tackle | پیاده‌سازی‌شده و تست‌شده | رویدادهای Pass/Kick/Tackle + تست مسابقه | — |
| گل و دروازه‌بان | پیاده‌سازی‌شده، تست‌شده؛ keeper ضعیف | تست گل/CCD تیرک؛ بنر GOAL! در فریم | تعادل keeper |
| پایان مسابقه / برد-باخت | پیاده‌سازی‌شده | ساعت ۳۰۰s + teamScore | صفحهٔ پایان |
| HUD | پیاده‌سازی‌شده | فریم: «MA 0 - 1 ANHA 4:54» | — |
| pause / restart | ناقص | pause ویرایشگر هست؛ منوی درون‌بازی نیست | منوی بازی |
| save/load | پیاده‌سازی‌شده (world) / وجود ندارد (progress) | round-trip بایت‌یکسان | checkpoint مسابقه |
| replay | **وجود ندارد** | grep	CameraTrack/Replay خالی | فاز ۷ |
| حرکات نمایشی | پیاده‌سازی‌شده، تست‌نشده | `startTrick` + ۳ ثابت زمانی (World.cpp:1177) | تست + انیمیشن |
| شخصیت‌های متفاوت | پیاده‌سازی‌شده | ۳ mesh کودک در street_match | تنوع بیشتر |
| داستان / دیالوگ | ناقص | سیستم Dialogue داده‌محور هست؛ محتوای داستان نیست | فاز ۸ |
| اتفاقات محیطی | وجود ندارد | grep EnvironmentEvent = 0 | فاز ۸ |
| زمین خیابانی / چمن | پیاده‌سازی‌شده و تست‌شده | street_match + ماده‌ها | — |
| حالت خیلی سریع / کمدی / ارز | وجود ندارد | — | طراحی بازی |

## 9. وضعیت build و CI

| مورد | وضعیت |
| --- | --- |
| Linux (-Werror + ctest) | Pass — محلی (build-review) و CI روی `0e46a83` |
| Windows | Pass در CI (MSVC smoke + EXE self-contained)؛ اجرا روی Windows: Not run |
| Android | Pass در CI (APK build)؛ اجرا روی دستگاه: Not run |
| WebGL | Pass در CI (WASM smoke build)؛ اجرا در مرورگر: Not available هنا |
| Sanitizer | Pass — build مأموریت (Debug ASan/UBSan) ctest 100% (102s) + jobهای CI؛ TSان پیش‌تر سبز |
| Tests | Pass — 565/565 محلی و ctest |
| check runs | success × 3 workflow روی `0e46a83`؛ روی `10b9ffe` شکست واقعی گرفت و اصلاح شد |

## 10. برنامهٔ ۱۴ روزه

1. روز ۱–۳: تعادل keeper (lane + dive timing) + تست «مسابقهٔ ۳ دقیقه‌ای باید بین ۰–۶ گل تمام شود».
2. روز ۴–۷: فاز ۵ بخش اول: ماشین حالت گیت (Idle/Walk/Run/Sprint/Stop/Turn) از velocity واقعی + blend شتاب/کاهش + تست foot-sliding روی توقف ناگهانی.
3. روز ۸–۹: foot IK ساده روی زمین ناهموار + contact زمان ضربه (Pass/Shoot) + تست زمان‌بندی.
4. روز ۱۰: fuzz harness برای SceneIO/WorldIO/profile (ورودی تصادفی → crash ممنوع).
5. روز ۱۱: golden-image فایل برای رستر نرم‌افزاری +_compare در ctest.
6. روز ۱۲: Android emulator smoke در CI یا چک‌لیست دستی مستند + حذف symbol مرده.
7. روز ۱۳: auth فقط در header (حذف توکن از URL) + تست.
8. روز ۱۴: bench/report هفتگی + بستن یک release tag با همهٔ gateها سبز.

## 11. برنامهٔ ۶۰ روزه (vertical slice)

- هفته ۱–۲: همان ۱۴ روز بالا (پایهٔ قابل تماشا).
- هفته ۳–۴: فاز ۶ کامل: ۹ نقش، صفات seed-محور serializeشده، تاکتیک تیمی؛ تست determinism seed.
- هفته ۵–۶: فاز ۷: فرمت replay (version/seed/inputs/events) + دوربین‌های Follow/Broadcast/GoalReplay + تست بازتولید بیت‌یکسان.
- هفته ۷: فاز ۸ محتوا: ۳ شخصیت، ≥۱۰ دیالوگ داده‌محور، ≥۳ رویداد محیطی، skill moveها با انیمیشن و تست.
- هفته ۸: فاز ۹ editor: undo/redo، multi-select، AI/physics debug view در UI.
- پایان: vertical slice ۱۰ دقیقه‌ای قابل برد-باخت، save/load/checkpoint، replay یک گل، روی Linux و Windows executable.

## 12. verdict نهایی

**«با اصلاح blockerها merge شود»** — دو blocker واقعی یافت‌شده حین بررسی همین‌جا اصلاح، تست و
push شدند و CI روی tip نهایی سبز است. توجه اجرایی: PR #8 خودش CLOSED است و diff خالی دارد؛
وسیلهٔ درست merge، یک PR تازه از `arena/01a0c3a7-kimia-engine` به `feature/engine-10-of-10` است.

## 13. معیار رسیدن به ۱۰ از ۱۰ (اندازه‌گیرپذیر)

1. Linux clean build با -Werror و ctest سبز روی tip.
2. ASan+UBSan و TSan سبز در CI.
3. APK روی emulator/device واقعاً launch شود و ۶۰ ثانیه frame بدهد.
4. نام loadLibrary و soname و همهٔ سمبل‌های JNI تست‌شده و سبز.
5. Windows exe اجرا شود و --version بدهد.
6. WebGL در مرورگر frame بدهد.
7. replay با seed یکسان، مسابقه را بیت‌یکسان بازتولید کند (تست).
8. مسابقهٔ v3 قابل بازی با برد/باخت و پایان مسابقه.
9. keeper طوری تنظیم شود که مسابقهٔ ۳ دقیقه‌ای ≤۶ گل داشته باشد (تست عددی).
10. انیمیشن از velocity واقعی، بدون foot sliding معنادار روی توقف (تست زمانی).
11. save/load/checkpoint بدون data loss (تست round-trip).
12. editor: undo/redo و multi-select با تست endpoint.
13. physics bench: ۱۰۰۰ جسم < 8 ms/step و گزارش CI.
14. fuzz چهار پارسر بدون crash در ۱۰⁶ input.
15. هیچ bind بدون auth و هیچ توکن در URL لاگ‌ها.
