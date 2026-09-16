# اجرای موتور KIMIA روی Termux (اندروید)

این راهنما فرض میکند یک گوشی اندروید با **Termux** نصب‌شده داری و میخواهی موتور را همان‌جا بسازی و اجرا کنی — بدون WebView، بدون نصب APK، فقط خود موتور روی یک ترمینال.

> **نکتهٔ معماری:** APK بومی اندروید (که در [Android.md](Android.md) توضیح داده شده) موتور را داخل یک Activity جاوا اجرا میکند و روی `SurfaceView` رندر میگیرد. مسیر Termux یک راه دوم است: همان باینری `kimia_world` که روی لینوکس/ویندوز اجرا میشود را روی گوشی بالا میآوریم. رندر GLES3 همان است (EGL روی `OSWindow` ساختگی)، فقط بدون پنجرهٔ واقعی: خروجی به‌صورت JPEG استریم میشود و داخل یک صفحهٔ HTML در مرورگر گوشی نمایش داده میشود (`http://127.0.0.1:8080`).

---

## ۱. پیش‌نیاز

- **Termux** (از F-Droid، نه Play Store).
- اینترنت در گوشی (برای دانلود پکیجها و clone کردن ریپو).
- **حداقل ۱.۵ گیگابایت** فضای آزاد (Termux + clang + cmake + ninja + سورس + بیلد).
- گوشی **ARM64** (همهٔ گوشیهای ۲۰۱۸ به بعد). ARM 32-bit هم کار میکند ولی کندتر.

## ۲. آماده‌سازی Termux (یک بار، یک دقیقه)

> اگر قبلاً Termux را آپدیت کردهای، از مرحلهٔ ۳ برو.

```bash
# به‌روزرسانی مخازن (Play Store نسخهٔ Termux قدیمی است؛ فقط F-Droid)
pkg update -y && pkg upgrade -y

# ابزارهای لازم
pkg install -y git clang cmake ninja python
```

پس از آن:

```bash
termux-setup-storage   # اختیاری، فقط اگر میخواهی فایلها در sdcard ذخیره شوند
```

## ۳. کلون کردن ریپو

شاخهٔ کاری **همیشه** `arena/01a080a4-ai-codespace` است. `main` فقط اسکلت است و موتور کامل را ندارد:

```bash
cd ~
git clone https://github.com/javadxpro/AI-codespace.git
cd AI-codespace
git checkout arena/01a080a4-ai-codespace
```

> اگر قبلاً clone کردهای: `cd AI-codespace && git fetch && git checkout arena/01a080a4-ai-codespace && git pull`.

## ۴. بیلد (یک دستور، ۵ تا ۱۵ دقیقه)

```bash
bash Tools/termux_build.sh
```

این اسکریپت:

1. چک میکند که سورس کامل موتور هست (روی شاخهٔ درست).
2. اگر چیزی نصب نبود، با `pkg install` نصب میکند.
3. `cmake -B build -DKIMIA_WERROR=ON -DCMAKE_BUILD_TYPE=Release`.
4. `cmake --build build -j$(nproc)`.
5. `build/bin/kimia_tests` را اجرا میکند. باید **475/475** سبز بگوید.

اگر وسط کار قطع شد، دوباره همان دستور را بزن — `build/` باقی مانده و ادامه میدهد.

## ۵. اجرا

```bash
./build/bin/kimia_world --port 8080 --profiles build/bin/profiles
```

خروجی:

```
[INFO] KIMIA 0.29.0 — listening on http://127.0.0.1:8080
```

حالا در همان گوشی (یا هر دستگاه دیگر روی همان Wi-Fi)، مرورگر را باز کن:

```
http://127.0.0.1:8080
```

> **نکته:** در Termux نیازی به `play.sh` یا `--webgl` نیست. این مسیر WebViewer/HTTP است که همان کار را میکند ولی روی EGL/GLES3 (نه WebGL). فریمها بهصورت JPEG روی `/stream` استریم میشوند.

## ۶. بازیها

بهصورت پیشفرض چهار بازی داخل موتور هست (golf / street / grass / battleground). URL:

```
http://127.0.0.1:8080/?game=golf
http://127.0.0.1:8080/?game=street
http://127.0.0.1:8080/?game=grass
http://127.0.0.1:8080/?game=battleground
```

> پارامتر `?game=...` همان اولویت اول را از بین بازیهای موجود در پروفایلهای داخلی انتخاب میکند.

## ۷. اجرا + بیلد با یک دستور

```bash
bash Tools/termux_build.sh --run
```

این بیلد میکند و در پایان خود `kimia_world` را اجرا میکند (Ctrl+C برای توقف).

## ۸. تغییر پورت (اگر 8080 اشغال بود)

```bash
bash Tools/termux_build.sh --run --port=9090
```

## ۹. بیلد تمیز (اگر بیلد قدیمی خراب شد)

```bash
bash Tools/termux_build.sh --clean
```

---

## Troubleshooting

| علامت | علت | راهحل |
| --- | --- | --- |
| `cmake: command not found` | نصب نیست یا خراب است | `pkg install -y cmake`؛ اگر خطای `CMAKE_ROOT` چاپ شد، `pkg uninstall cmake && pkg install cmake` |
| `fatal error: 'jni.h' not found` | شاخه اشتباه clone شده | `git branch` باید `arena/01a080a4-ai-codespace` باشد، نه `main` |
| `./build/bin/kimia_tests failed` | سورس ناقص | `git checkout arena/01a080a4-ai-codespace && git pull && bash Tools/termux_build.sh --clean` |
| مرورگر چیزی نشان نمیدهد | فایروال / حالت privacy مرورگر | اگر از Firefox استفاده میکنی، Enhanced Tracking Protection را خاموش کن |
| فریمها خیلی کند هستند | GLES3 در CPU روی برخی گوشیها | `--no-gpu` اضافه کن (فقط WebViewer headless) |
| گوشی داغ میکند | `--fps=30` بزن | `./build/bin/kimia_world --port 8080 --fps 30` |
| خطای OpenGL ES روی گوشیهای خیلی قدیمی | پشتیبانی نکردن GPU از GLES3 | همان `--no-gpu` یا `--swrast` |

## تشخیص نسخه

```bash
./build/bin/kimia_world --version
```

باید چاپ کند: `KIMIA 0.29.0 (engine: GLES3 headless)`.

## گام بعدی

- برای ساختن **APK** (نصب‌پذیر روی گوشی) به [Android.md](Android.md) مراجعه کن.
- برای ساختن **EXE ویندوز** به [Windows.md](Windows.md) مراجعه کن.
- برای اتصال از گوشی دیگر روی Wi-Fi:

```bash
# IP گوشی Termux را پیدا کن
ip addr show | grep -w inet

# روی آن IP
http://192.168.x.x:8080
```

> **نکتهٔ امنیتی:** WebViewer هیچ احراز هویتی ندارد. هر کسی روی شبکهٔ Wi-Fi شما میتواند به آن وصل شود. اگر نگرانی، فقط روی حالت آفلاین استفاده کن، یا با `iptables -A INPUT -p tcp --dport 8080 -j DROP` پورت را ببند.

---

## خلاصهٔ ۵ دقیقهای

```bash
pkg update -y && pkg upgrade -y
pkg install -y git clang cmake ninja python
cd ~ && git clone https://github.com/javadxpro/AI-codespace.git
cd AI-codespace && git checkout arena/01a080a4-ai-codespace
bash Tools/termux_build.sh --run
# سپس در مرورگر: http://127.0.0.1:8080
```
