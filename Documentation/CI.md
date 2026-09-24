# CI، بیلد و تست (فاز ۱)

این سند «قرارداد سبز بودن» KIMIA است: چه چیزی بیلد می‌شود، چه چیزی تست
می‌شود، و چطور همان کاری را که CI انجام می‌دهد روی ماشین خودت تکرار کنی.
هدف فاز ۱ این بود که **ادعای** سبز بودن جای خود را به **گزارش واقعی** بدهد:
هر عددی که پایین می‌آید از اجرای همین ابزارها روی همین درخت کد آمده است.

> **وضعیت Actions (از 0.30):** به تصمیم مالک پروژه (پلن رایگان GitHub) همهٔ
> workflowها غیرفعال شدند و فایل‌هایشان با پسوند `.disabled` در
> `.github/workflows/` بایگانی شده است — نه حذف، تا روزی که خواستی همان‌ها
> برگردند. از این commit به بعد هیچ push دقیقه‌ای از Actions نمی‌سوزاند.
> پیامد صادقانه‌اش: **gate مرجع، اجرای محلی است** (`Tools/run_tests.sh` و
> `--sanitize` / `--tsan`) و ستون‌های Windows / Android / WebGL در گزارش‌ها
> از این به بعد «تأییدنشده در این محیط» خوانده می‌شوند، مگر خودت روی آن
> دستگاه‌ها اجرا کنی. آخرین اجرای سبز Actions روی commit `1754c63` بود.

---

## ۱. یک دستور برای همهٔ تست‌ها

```bash
bash Tools/run_tests.sh                 # Release، بدون SDL، -Werror، ctest
bash Tools/run_tests.sh --sanitize      # ASan + UBSan
bash Tools/run_tests.sh --clean         # از صفر (مثل clean checkout)
bash Tools/run_tests.sh --build-dir out/release -j 8
```

اسکریپت در یک پوشهٔ بیلد مستقل کانفیگ می‌کند، می‌سازد، و `ctest` را اجرا
می‌کند؛ با `--clean` اول پوشه را پاک می‌کند تا همان مسیر یک checkout تازه
آزمایش شود. کد خروج ۰ یعنی همه‌چیز سبز.

همان کار با CMake:

```bash
cmake --preset linux-release            # -Werror، SDL2=OFF، Release
cmake --build --preset linux-release -j4
ctest --preset linux-release --output-on-failure

cmake --preset linux-sanitizers         # ASan + UBSan
cmake --build --preset linux-sanitizers -j4
ctest --preset linux-sanitizers --output-on-failure
```

و داخل خود بیلد، target آمادهٔ `check`:

```bash
cmake --build build --target check      # بیلد + اجرای کامل تست‌ها
```

---

## ۲. گزینه‌های بیلد مربوط به کیفیت

| گزینه | پیش‌فرض | کار |
| --- | --- | --- |
| `KIMIA_WERROR` | `ON` | `-Werror` (GCC/Clang) یا `/WX` (MSVC) |
| `KIMIA_SANITIZE` | `OFF` | `OFF` / `ADDRESS_UNDEFINED` / `THREAD` |
| `KIMIA_BUILD_TESTS` | `ON` (روی ویندوز `OFF`) | ساخت `kimia_tests` و target `check` |
| `KIMIA_ENABLE_SDL2` | `ON` | پنجرهٔ بومی؛ `OFF` = مسیر headless/WebViewer |
| `KIMIA_RENDER_BACKEND` | `AUTO` | `AUTO` / `D3D11` / `OPENGL` / `SOFTWARE` |

`KIMIA_SANITIZE` روی کل درخت کد (شامل کتابخانه‌های vendored) اعمال می‌شود،
به‌جز یک فایل که در بند ۴ توضیح داده شده.

---

## ۳. کارهایی که CI اجرا می‌کند

| job | پایه | چه چیزی را ثابت می‌کند |
| --- | --- | --- |
| `linux-gcc` | ubuntu-24.04، CMake+Ninja | بیلد Release با `-Werror` + کل CTest + smoke اجرای واقعی اپ (زیر) |
| `sanitizers` (ASan+UBSan) | همان + `-DKIMIA_SANITIZE=ADDRESS_UNDEFINED` | نبود خطای حافظه/UB در کل تست‌ها |
| `sanitizers` (TSan) | همان + `-DKIMIA_SANITIZE=THREAD` | نبود data race در نخ‌های سرور/لوپ |
| `windows-msvc-smoke` | windows-2022، MSVC | کامپایل کل درخت با MSVC/W4 + `--version` اجرا می‌شود |
| `wasm-smoke` | emsdk 3.1.61 | `Examples/WebGLApp.cpp` با emcmake ساخته می‌شود و `.html` می‌دهد |
| `android-apk` | ubuntu + NDK 26.3 + Gradle 8.7 | `kimia_jni` و APK دیباگ ساخته می‌شوند |
| `windows-exe` | windows-2022 + vcpkg SDL2 static | EXE خودکفای تک‌فایلی با دارایی‌های جاسازی‌شده |

حالت TSan در همین سند جلوتر (بند ۶) به‌عنوان «وارد CI نمی‌شود» نوشته شده بود؛
پس از رفع مسابقهٔ واقعی که خودش پیدا کرد (`web::Server::stop`)، تمیز شد و
اکنون یک job لازم‌الاجراست.

`wasm-smoke` تا اولین اجرای سبز، `continue-on-error: true` داشت (در این
محیط Emscripten نصب نیست). در اجرای CI شمارهٔ 35596687112 هر پنج job سبز
شدند، پس آن استثنا برداشته شد و این job هم گیت است. همان job دو باگ واقعی
پیدا کرد که هیچ کامپایلری قبلاً ندیده بود:
`Examples/WebGLApp.cpp` هرگز کامپایل نشده بود (یک پرانتز جابه‌جا) و
`handle_` در `GLFunctions.h` زیر Emscripten بی‌استفاده می‌شد.

### smoke اجرای اپ (فاز ۲)

`kimia_tests` موتور را لینک می‌کند، نه اپ را: نه سرور را بالا می‌آورد، نه
فریم می‌سازد و نه پروفایل می‌خواند. پس هر کرشی در مسیر راه‌اندازی
`Examples/WorldEditorApp.cpp` برای تست‌ها نامرئی است — و همین‌طور بود: یک
اشاره‌گر آویزان به پروفایل داخل fallback راه‌اندازی، ماه‌ها زنده ماند و فقط
وقتی کسی اپ را با `--assets`ی اجرا کرد که `../Worlds/street_kids.kimia`
نداشت، ظاهر شد (ASan: `SEGV` داخل `WorldEditor::createWorld`).

بنابراین `linux-gcc` یک قدم آخر دارد: همان باینری واقعی را با
`--assets Tests/assets --profiles Profiles` (که عمداً مسیر fallback را
می‌رود) بالا می‌آورد، تا ۲۰ ثانیه دنبال `GET /frame.jpg` می‌گردد و مطمئن
می‌شود (الف) پروسه زنده است و (ب) فریم غیرخالی می‌دهد. اگر اپ بمیرد،
`app.log` در همان قدم چاپ می‌شود.

هیچ‌کدام به secret نیاز ندارند؛ فقط `GITHUB_TOKEN` پیش‌فرض برای checkout.

---

## ۴. استثناهای مستند (نه پنهان)

1. **`ThirdParty/stb/stb_image_write_impl.c`** با
   `-fno-sanitize=shift-base,shift-exponent` کامپایل می‌شود. نویسندهٔ JPEG در
   stb عمداً بیت علامت را با شیفت چپ پر می‌کند و UBSan آن را undefined
   می‌داند. این فایل جداست تا **هر خطی که خودمان نوشتیم** زیر مجموعهٔ کامل
   sanitizer بماند. (بقیهٔ کد vendored — `dr_impl.c` و `stb_image_impl.c` —
   زیر sanitizer کامل هستند.)
2. **پیاده‌سازی کتابخانه‌های تک‌فایلی** (`ThirdParty/dr/dr_impl.c`،
   `ThirdParty/stb/stb_image_impl.c`، `stb_image_write_impl.c`) با
   `KIMIA_STRICT_WARNINGS` و `-Werror`/`/WX` کامپایل **نمی‌شود**؛ فقط
   اعلان‌های همان هدرها وارد کد ما می‌شوند. دلیل واقعی: در ویندوز،
   `dr_wav.h` یک متغیر «ممکن است مقدار نگیرد» دارد (`fileSize`) که با
   `/W4 /WX` بیلد را می‌شکند و `#pragma warning(push, 0)` هم آن را
   نمی‌خواباند. نتیجه: کد ما زیر گیت سخت‌گیر می‌ماند و کد vendored نه؛
   هیچ warningی پنهان نمی‌شود، فقط جای شکل‌گیری‌اش عوض شده است.
3. **`Tests/src/WebTests.cpp`** مستقیم از سوکت POSIX استفاده می‌کند، پس
   `kimia_tests` روی ویندوز ساخته نمی‌شود؛ به همین دلیل `windows-msvc-smoke`
   فقط بیلد و `--version` را می‌سنجد. پورت کردن آن کار جداگانه‌ای است و در
   ROADMAP ثبت شده است.

---

## ۵. اجرای تست‌ها در یک checkout تمیز

`Tools/run_tests.sh --clean` پوشهٔ بیلد را پاک می‌کند و از صفر می‌سازد.
آزمایش کامل‌تر (کلون تازه) این است:

```bash
git clone https://github.com/javadxpro/KIMIA-ENGINE.git /tmp/kimia-clean
cd /tmp/kimia-clean && bash Tools/run_tests.sh --clean
```

دارایی‌های کوچک تست (`Tests/assets/tone.wav`, `2x3.png`, `2x2.jpg`,
`cube.obj`, `quad.obj`) با هر بیلد توسط `Tools/src/asset_gen.cpp` ساخته
می‌شوند و در گیت ignore هستند؛ بقیهٔ فایل‌های `Tests/assets` در مخزن هستند.

---

## ۶. یافته‌های واقعی همین زیرساخت (نه ادعا)

با روشن کردن sanitizer، دو مانع و یک باگ واقعی پیدا شد و هر سه رفع شدند:

1. `ThirdParty/ufbx/ufbx.c` هنگام خواندن FBX زیر UBSan می‌مرد. **کلید رسمی
   خودِ ufbx** (`UFBX_UBSAN`، بالای همان فایل) این کار را حل می‌کند؛ clang
   خودش تشخیص می‌دهد، ولی GCC برای `-fsanitize=undefined` هیچ ماکرویی تعریف
   نمی‌کند، پس در `CMakeLists.txt` صریح پاس داده می‌شود.
2. `ThirdParty/stb/stb_image_write.h` در نویسندهٔ JPEG بیت علامت را با شیفت
   چپ پر می‌کند (تصمیم عمدی stb). آن فایل به یک TU جدا منتقل شد تا تنها
   تخفیفِ sanitizer روی کد vendored باشد، نه روی کد ما.
3. TSan یک **data race واقعی** در `kimia::web::Server::stop()` پیدا کرد:
   `listenFd` هم‌زمان در نخ پذیرش خوانده و در `stop()` نوشته می‌شد. هنگام رفع،
   دو مشکل همسایه هم درست شد: `close()` قبل از `join()` صدا می‌شد (خطر بازاستفادهٔ
   توصیفگر توسط نخ دیگر) و handlerهای detached می‌توانستند بعد از آزاد شدن
   `Impl` به حافظهٔ آزادشده دست بزنند (اکنون `Impl` با `shared_ptr` زنده
   نگه داشته می‌شود).

## ۷. چه چیزی هنوز نیست

- تست‌ها روی ویندوز اجرا نمی‌شوند؛ آن job فقط smoke است (بند ۴-۲).
- smoke اجرای اپ فقط روی لینوکس و فقط «بالا می‌آید و فریم می‌دهد» را می‌سنجد؛
  تعامل و رندر GL/D3D11 هنوز گیت ندارد (فاز ۸ و ۹).
- `wasm-smoke` تا اولین اجرای سبز، advisory است.
- تنظیم‌کننده‌های سرور WebViewer (`setPage`/`setMenu`/`setApiHandler`/…)
  فقط در زمان راه‌اندازی، پیش از سرو کردن ترافیک، ایمن‌اند؛ خواندن‌های
  handler قفل نمی‌گذراند. این در فاز ۱۰ (سخت‌سازی WebWorkbench) بسته می‌شود.
- fuzz و golden-image در فاز ۱۰ می‌آیند.
