# معماری

لایه‌های موتور KIMIA و جریان داده میان آن‌ها.

---

## لایه‌ها (از پایین به بالا)

| لایه | مسئولیت |
| --- | --- |
| `Engine/Core` | انواع عددی، گزارش، پروفایلر، گام ثابت |
| `Engine/Math` | بردار، ماتریس، کواترنیون، دوربین — header-only، بدون وابستگی |
| `Engine/Graphics` | `MeshData` و پریمیتیوها، بیت‌مپ‌فونت، اسکلت/انیمیشن، `Image` |
| `Engine/Assets` | خط لولهٔ دارایی: OBJ/FBX، تصویر PNG/JPG، صدا WAV/MP3/OGG/FLAC |
| `Engine/Scene` | موجودیت‌ها (هندل ۱‌مبنا) و `SceneIO` (فرمت متنی v1) |
| `Engine/Physics` | فیزیک گام‌ثابت + کنترل‌گر کاراکتر |
| `Engine/Renderer` | GL/GLES3/WebGL2/D3D11 + رسترایزر نرم‌افزاری |
| `Engine/Platform` | ورودی (LEVEL/EDGE) + پنجرهٔ اختیاری SDL2 |
| `Engine/App` | بوت‌استرپ `Engine`، WebViewer، دارایی‌های جاسازی‌شده |
| `Engine/Golf` | بازی مرجع گلف (`GolfGame` + سازندهٔ زمین) |
| `Engine/World` | `WorldEditor` — شبیه‌سازی بازی و پرسش/پاسخ سازنده (چند فایل، بخش پایین) |
| `Engine/Profile` | `GameProfile` + فرمت `*.kimiaprofile` |
| `Engine/Raytracer` | ردیاب پرتو آفلاین (PBR + GI + BVH) |

## وابستگی‌ها

فقط رو به بالا است؛ لایه‌های پایین چیزی از بالایی نمی‌دانند:

```
Core → Math → Graphics/Assets/Scene → Physics/Renderer → World/Profile → App
```

دو نقطهٔ عمدیِ جداسازی:

- **رندر** پشت `Renderer`/`renderSoftware` پنهان است؛ `World`/`App` فقط یک `RenderScene` می‌سازند و نمی‌دانند با GL، GLES3، D3D11 یا CPU رسم می‌شود.
- **منطق بازی** در `LogicRuntime` می‌گوید *چه* باید بشود؛ دنیا آن را انجام می‌دهد. منطق بدون رندر، پنجره یا فیزیک تست می‌شود.

## داخل `Engine/World`

‏`WorldEditor` یک کلاس است، ولی دیگر یک فایل نیست. هر فایل یک **حوزهٔ
مسئولیت** است و نامش می‌گوید کدام:

| فایل | مسئولیت |
| --- | --- |
| `World.cpp` | چرخهٔ عمر دنیا، ساخت/بارگذاری/ذخیره، `update` و حلقهٔ بازی، شوت/پاس/ترفند، توپ و جعبه‌ها |
| `WorldBuilder.cpp` | صفحه‌ها و منوهای سازنده (`choose`, `press`, `beginPlace`, …) |
| `WorldIO.cpp` | فایل `.kimia` (سرصفحهٔ دنیا + `SceneIO`) |
| `Rules.cpp` | قاعده‌های بازی: توقف‌ها، آفساید، استقامت، سرعت بازیکن، `updateRules` |
| `Ai.cpp` | بازیکنان کامپیوتری: توپ‌گیر/دروازه‌بان/نقش/فاصلهٔ شخصی/`updateAi` |
| `Animation.cpp` | کلیپ‌ها و поз: `playClip`، اسکلت FBX و ریگ دست‌ساز |
| `Camera.cpp` | سوژهٔ دوربین و فاصله‌اش (ریگ در `Engine/View`) |
| `Dialogue.cpp` | خط‌های گفتاری: کامپوننت، تریگر، کپشن |
| `Arena.cpp` | حالت آخرین‌بازمانده: جان، مهمات، راند |
| `Logic.cpp` | اجرای کتاب منطق (`LogicRuntime`) |
| `Picking.cpp` | انتخاب با پرتو/صفحه |
| `Hud.cpp` | چیدمان HUD و ویجت‌ها |
| `Particles.cpp` | افکت‌های ذره‌ای |
| `Assets.cpp` | کاتالوگ دارایی و مدل‌های قابل‌گذاشتن |
| `Input.cpp` | نگاشت ورودی به صفحه‌ها |
| `Studio.cpp` | سرور WebWorkbench: HTML/JS و `/api/*` |
| `WorldInternal.h` | ابزار خصوصی ماژول (constant و helper مشترک). **API نیست** |

قاعده‌ها:

* `WorldInternal.h` خصوصی است: نه install می‌شود، نه از بیرون `Engine/World/src`
  include می‌شود. هرچه در `World.h` است API است؛ هرچه در `worldinternal` است
  جزئیات پیاده‌سازی.
* شکستن‌ها **بدون تغییر رفتار** انجام شد: متن جابه‌جا شده بایت‌به‌بایت همان
  است. سنجشش یک مقایسهٔ خط‌به‌خط با کامیت قبلی است (صفر خط گم‌شده/تغییریافته)،
  به‌علاوهٔ همان ۵۲۹ تست.

## جریان یک فریم (Play)

```
poll ورودی (پنجره/وب/لمس) → editor.update(گام ثابت) → buildScene → render → HUD → present
```

حلقهٔ مرجع در `Examples/WorldEditorApp.cpp` است؛ نسخهٔ اندروید همان گام‌ها را در `Android/app/src/main/cpp/jni_glue.cpp` اجرا می‌کند (لمس به `WorldEditor` + `OrbitCamera` نگاشت می‌شود).

## قراردادها

- C++17، بدون استثنا در مسیر داغ، `-Werror` (کلنگ/GCC) و `/W4 /WX` (MSVC).
- شماره‌گذاری معنایی نسخه در `Version.h`؛ دفترچهٔ `CHANGELOG.md` با نسخهٔ فعلی هم‌خوان است (تست دارد).
- هر زیرسیستم پیش از رفتن به مرحلهٔ بعد، تست مستقل دارد.

---
