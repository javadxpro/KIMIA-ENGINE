/* dr_libs audio decoders (WAV / MP3 / FLAC), implementation only.
 *
 * These used to be expanded inside Engine/Assets/src/Audio.cpp, which meant
 * every warning in ~20k lines of vendored C had to be silenced around the
 * includes. MSVC's warning-level pragmas do not reliably cover the
 * maybe-uninitialized class of diagnostics the decoders produce
 * (dr_wav.h: a `fileSize` local that is only written when onTell succeeds),
 * so /W4 + /WX failed the Windows build on vendored code.
 *
 * Keeping the implementations in a vendored translation unit removes the
 * problem at the root: this file is not compiled with KIMIA_STRICT_WARNINGS or
 * the warning-as-error switch, while Audio.cpp keeps only the declarations.
 * The dr_ headers wrap everything in `extern "C"`, so C++ callers link
 * unchanged. Sanitizers still apply here (KIMIA_SANITIZE is directory-wide).
 */
#define DR_WAV_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>
#include <dr_mp3.h>
#include <dr_wav.h>
