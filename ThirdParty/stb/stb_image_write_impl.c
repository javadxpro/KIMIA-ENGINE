/* stb_image_write implementation, in its own translation unit.
 *
 * Upstream stb_image_write.h is a single-file library: the implementation is
 * only emitted where STB_IMAGE_WRITE_IMPLEMENTATION is defined, which used to
 * be inside Engine/Graphics/src/Image.cpp. Keeping it here has two benefits:
 *
 *   1. stb's JPEG bit writer left-shifts into the sign bit on purpose
 *      (stbiw__jpg_writeBits: `bitBuf |= bs[0] << (24 - bitCnt)`), which
 *      UndefinedBehaviorSanitizer reports as "left shift of negative value".
 *      Scoping the relaxation to this one vendored file keeps every line we
 *      wrote under the full sanitizer set instead of weakening the whole
 *      image reader. See CMakeLists.txt (kimia_stb_image_write).
 *   2. Editing Image.cpp no longer recompiles the whole of stb.
 *
 * The declarations stay in the header, so Image.cpp keeps including
 * <stb_image_write.h> as before.
 */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
