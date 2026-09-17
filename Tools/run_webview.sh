#!/bin/bash
# Quick launcher for the WebViewer.
#
# 1) build the engine if ./kimia_street_soccer is missing
# 2) pick a writable output directory
# 3) start the engine in trailer+web mode in the background, telling it to
#    write snapshots to that directory
# 4) start a Python HTTP server on :8765 rooted at the same directory
# 5) print the local URL
#
# Environment / overrides:
#   KIMIA_WEB_PORT   port to listen on (default 8765)
#   KIMIA_WEB_OUT    override the snapshot directory entirely

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${KIMIA_WEB_PORT:-8765}"

choose_out() {
  if [ -n "$KIMIA_WEB_OUT" ]; then echo "$KIMIA_WEB_OUT"; return; fi
  if [ -n "$XDG_RUNTIME_DIR" ] && mkdir -p "$XDG_RUNTIME_DIR/kimia-web" 2>/dev/null; then
    echo "$XDG_RUNTIME_DIR/kimia-web"; return
  fi
  if mkdir -p "$HOME/kimia-web" 2>/dev/null; then
    echo "$HOME/kimia-web"; return
  fi
  if mkdir -p "/tmp/kimia-web" 2>/dev/null; then
    echo "/tmp/kimia-web"; return
  fi
  echo "ERROR: no writable directory found for snapshots" >&2
  exit 1
}
OUT="$(choose_out)"

if [ ! -x "$ROOT/kimia_street_soccer" ]; then
  bash "$ROOT/Tools/termux-build.sh" || exit 1
fi

echo "Output directory: $OUT"
rm -rf "$OUT"
mkdir -p "$OUT"
cp "$ROOT/Tools/web/index.html" "$OUT/"

# --no-loop keeps the trailer finite (one pass through the script);
# omit it (or set KIMIA_WEB_LOOP=0) for an infinite demo loop.
LOOP_FLAG=""
if [ "${KIMIA_WEB_LOOP:-1}" != "0" ]; then
  LOOP_FLAG="--loop"
fi
echo "Loop mode: ${LOOP_FLAG:-off}"

# Tell the engine where to write state.json (it reads KIMIA_WEB_OUT).
export KIMIA_WEB_OUT="$OUT"
"$ROOT/kimia_street_soccer" --trailer --web $LOOP_FLAG \
  > "$OUT/engine.log" 2>&1 &
ENGINE_PID=$!
sleep 1

cd "$OUT"
python3 -m http.server "$PORT" > "$OUT/server.log" 2>&1 &
HTTP_PID=$!

cat <<EOF

  WebViewer is up.

    • engine pid:    $ENGINE_PID
    • http pid:      $HTTP_PID
    • URL:           http://localhost:$PORT/
    • snapshots:     $OUT/state.json
    • engine log:    $OUT/engine.log
    • http log:      $OUT/server.log

  Press Ctrl-C to stop both.
EOF

cleanup() {
  echo "Stopping…"
  kill "$ENGINE_PID" "$HTTP_PID" 2>/dev/null || true
  # also kill any python http.server children that might have hung around
  pkill -f "http.server $PORT" 2>/dev/null || true
  wait 2>/dev/null || true
  echo "Done."
}

trap cleanup INT TERM EXIT

# Block until both children die. If one exits, tear down the other.
while kill -0 "$ENGINE_PID" 2>/dev/null && kill -0 "$HTTP_PID" 2>/dev/null; do
  sleep 1
done

echo "A child process exited; tearing down."
cleanup
