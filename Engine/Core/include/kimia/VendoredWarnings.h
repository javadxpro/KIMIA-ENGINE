#pragma once

// Silencing warnings from a vendored header — without silencing our own.
//
// The engine is built with -Wall -Wextra -Wconversion -Wsign-conversion
// -Wdouble-promotion and -Werror (GCC/Clang) or /W4 /WX (MSVC). The single-file
// libraries under ThirdParty/ predate those rules by years, so their headers are
// included between a push/pop pair:
//
//   #include <kimia/VendoredWarnings.h>
//
//   KIMIA_VENDORED_WARNINGS_PUSH
//   #include <stb_image.h>
//   KIMIA_VENDORED_WARNINGS_POP
//
// Only the region between the two macros is relaxed; everything the engine
// itself writes stays under the full gate. This lives in one place because the
// MSVC spelling is different: `#pragma GCC diagnostic` is not merely ignored by
// MSVC — it is warning C4068, which /WX then turns into a build failure. That
// is exactly what the first Windows CI run reported.
#if defined(_MSC_VER)
#define KIMIA_VENDORED_WARNINGS_PUSH __pragma(warning(push, 0))
#define KIMIA_VENDORED_WARNINGS_POP __pragma(warning(pop))
#elif defined(__GNUC__) || defined(__clang__)
#define KIMIA_VENDORED_WARNINGS_PUSH                     \
  _Pragma("GCC diagnostic push")                         \
  _Pragma("GCC diagnostic ignored \"-Wconversion\"")     \
  _Pragma("GCC diagnostic ignored \"-Wdouble-promotion\"") \
  _Pragma("GCC diagnostic ignored \"-Wshadow\"")         \
  _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"") \
  _Pragma("GCC diagnostic ignored \"-Wmissing-field-initializers\"")
#define KIMIA_VENDORED_WARNINGS_POP _Pragma("GCC diagnostic pop")
#else
#define KIMIA_VENDORED_WARNINGS_PUSH
#define KIMIA_VENDORED_WARNINGS_POP
#endif
