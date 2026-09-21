/* stb_image implementation, in its own translation unit.
 *
 * Same reason as stb_image_write_impl.c: a single-file library's
 * implementation belongs in one vendored file that is built without the
 * engine's strict warning gate, instead of being expanded inside a source file
 * whose surrounding code must stay warning-clean under -Werror / /WX.
 *
 * stb_image.h declares STBIDEF as `extern "C"` for C++ callers and reproduces
 * the same declaration list for C, so a C translation unit links straight
 * against Engine/Graphics/src/Image.cpp.
 *
 * NOTE: this target IS instrumented by KIMIA_SANITIZE (only the JPEG writer in
 * stb_image_write needs the documented shift relaxation).
 */
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
