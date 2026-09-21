#!/usr/bin/env bash
# KIMIA — build and run the whole test suite, from a clean tree if asked.
#
#   bash Tools/run_tests.sh                    # Release, -Werror, headless, ctest
#   bash Tools/run_tests.sh --sanitize         # the same under ASan + UBSan
#   bash Tools/run_tests.sh --tsan             # under ThreadSanitizer
#   bash Tools/run_tests.sh --debug            # Debug build
#   bash Tools/run_tests.sh --clean            # wipe the build dir first
#   bash Tools/run_tests.sh -j 8 --build-dir out/gcc
#
# This is the single entry point the brief asked for ("one target or script
# that runs the full test suite") and exactly what the CI linux/sanitizer jobs
# call, so a local run and a CI run cannot drift apart.
#
# Exit code is 0 only when configure, build AND tests all passed.
set -u
set -o pipefail

BOLD=$'\033[1m'; RED=$'\033[31m'; GREEN=$'\033[32m'; YELLOW=$'\033[33m'; RESET=$'\033[0m'
say()  { printf '%s==>%s %s\n' "$BOLD" "$RESET" "$*"; }
ok()   { printf '%s ok %s %s\n' "$GREEN" "$RESET" "$*"; }
warn() { printf '%s !! %s %s\n' "$YELLOW" "$RESET" "$*"; }
die()  { printf '%s XX %s %s\n' "$RED" "$RESET" "$*" >&2; exit 1; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || die "cannot cd to $ROOT"

BUILD_TYPE=Release
SANITIZE=OFF
CLEAN=0
RUN_BUILD=1
JOBS="$( (nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2) )"
BUILD_DIR=""
CONFIGURE_ONLY=0

while [ "$#" -gt 0 ]; do
  case "$1" in
    --sanitize|--asan|--asan-ubsan) SANITIZE=ADDRESS_UNDEFINED ;;
    --tsan) SANITIZE=THREAD ;;
    --debug) BUILD_TYPE=Debug ;;
    --release) BUILD_TYPE=Release ;;
    --clean) CLEAN=1 ;;
    --configure-only) CONFIGURE_ONLY=1 ;;
    --build-dir) shift; BUILD_DIR="${1:-}" ;;
    --build-dir=*) BUILD_DIR="${1#--build-dir=}" ;;
    -j|--jobs) shift; JOBS="${1:-2}" ;;
    -j*) JOBS="${1#-j}" ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) die "unknown option: $1 (try --help)" ;;
  esac
  shift
done

# A sanitizer build gets its own directory unless the caller named one, so an
# instrumented run never silently replaces the plain build tree.
if [ -z "$BUILD_DIR" ]; then
  case "$SANITIZE" in
    ADDRESS_UNDEFINED) BUILD_DIR="build-san" ;;
    THREAD)            BUILD_DIR="build-tsan" ;;
    *)                 BUILD_DIR="build" ;;
  esac
fi

# One toolchain note: the previous runs in this sandbox had a user-local cmake.
if ! command -v cmake >/dev/null 2>&1 && [ -x "$HOME/.local/bin/cmake" ]; then
  export PATH="$HOME/.local/bin:$PATH"
fi
command -v cmake >/dev/null 2>&1 || die "cmake not found (Termux: pkg install cmake; Debian: apt-get install cmake)"

say "checkout $ROOT ($(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo '?') @ $(git rev-parse --short HEAD 2>/dev/null || echo '?'))"
ENGINE_VERSION="$(grep -o 'KIMIA_ENGINE_VERSION "[0-9.]*"' Engine/Core/include/kimia/Version.h | grep -o '[0-9.]*')"
ok "engine ${ENGINE_VERSION:-?}, cmake $(cmake --version | head -1 | awk '{print $3}'), jobs $JOBS"

if [ "$CLEAN" = 1 ]; then
  say "removing $BUILD_DIR"
  rm -rf "$BUILD_DIR" || die "cannot remove $BUILD_DIR"
fi

GEN=()
command -v ninja >/dev/null 2>&1 && GEN=(-G Ninja)

mkdir -p "$BUILD_DIR" || die "cannot create $BUILD_DIR"
say "configure $BUILD_DIR (${BUILD_TYPE}, -Werror, SDL2=OFF, sanitize=${SANITIZE})"
cmake -S "$ROOT" -B "$BUILD_DIR" "${GEN[@]}" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DKIMIA_WERROR=ON \
  -DKIMIA_ENABLE_SDL2=OFF \
  -DKIMIA_SANITIZE="$SANITIZE" > "$BUILD_DIR/configure.log" 2>&1 || {
    cat "$BUILD_DIR/configure.log" >&2
    die "configure failed (log: $BUILD_DIR/configure.log)"
  }
ok "configured"

if [ "$CONFIGURE_ONLY" = 1 ]; then exit 0; fi

say "build -j$JOBS"
if ! cmake --build "$BUILD_DIR" -j"$JOBS" > "$BUILD_DIR/build.log" 2>&1; then
  tail -30 "$BUILD_DIR/build.log" >&2
  die "build failed (full log: $BUILD_DIR/build.log)"
fi
if grep -q 'warning:' "$BUILD_DIR/build.log"; then
  warn "$(grep -c 'warning:' "$BUILD_DIR/build.log") warning line(s) in the log — paste them, the engine promises zero"
else
  ok "0 warnings"
fi
[ -x "$BUILD_DIR/bin/kimia_tests" ] || die "$BUILD_DIR/bin/kimia_tests was not produced"

say "ctest"
ctest --test-dir "$BUILD_DIR" --output-on-failure || die "tests failed"
if [ "$SANITIZE" != "OFF" ]; then
  ok "sanitizer run (${SANITIZE}) found nothing"
fi
ok "done — ${BUILD_TYPE}${SANITIZE:+, sanitize=$SANITIZE}"
