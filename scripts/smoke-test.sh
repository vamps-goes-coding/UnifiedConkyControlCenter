#!/usr/bin/env bash
#
# Smoke test for UnifiedConkyControlCenter.
#
# Runs the binary's built-in `--smoke-test` self-check (which constructs the
# real main window, walks both modes and forces a full tab refresh) under a
# real display server, then asserts:
#
#   1. the process exited 0,
#   2. it actually reported success on stdout,
#   3. the session log contains no CRITICAL entries and no thrown exceptions.
#
# This replaces the old "did it survive N seconds?" check, which was wrapped in
# `|| true` and therefore could never fail.
#
# Usage: smoke-test.sh <path-to-binary> [x11|wayland|offscreen]
#
set -uo pipefail

BIN="${1:?usage: smoke-test.sh <path-to-binary> [x11|wayland|offscreen]}"
MODE="${2:-offscreen}"
TIMEOUT_SECS="${SMOKE_TIMEOUT:-60}"

if [ ! -x "$BIN" ]; then
    echo "::error::binary not found or not executable: $BIN" >&2
    exit 1
fi
BIN="$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"

OUT="$(mktemp)"
START_TS="$(date +%s)"
WESTON_PID=""
cleanup() {
    rm -f "$OUT" "$OUT.weston"
    [ -n "$WESTON_PID" ] && kill "$WESTON_PID" 2>/dev/null
    return 0
}
trap cleanup EXIT

WRAPPER=()
declare -a APP_ENV=()

echo "=== Smoke test ($MODE) ==="

case "$MODE" in
    x11)
        command -v xvfb-run >/dev/null 2>&1 \
            || { echo "::error::xvfb-run is not installed" >&2; exit 1; }
        WRAPPER=(xvfb-run -a -s "-screen 0 1280x800x24")
        ;;

    wayland)
        command -v weston >/dev/null 2>&1 \
            || { echo "::error::weston is not installed" >&2; exit 1; }

        : "${XDG_RUNTIME_DIR:=$(mktemp -d)}"
        export XDG_RUNTIME_DIR
        SOCKET="wayland-smoke-$$"

        weston --backend=headless-backend.so --width 1280 --height 800 \
               --socket="$SOCKET" >"$OUT.weston" 2>&1 &
        WESTON_PID=$!

        # Wait for the socket instead of sleeping a fixed amount.
        for _ in $(seq 1 30); do
            [ -e "$XDG_RUNTIME_DIR/$SOCKET" ] && break
            kill -0 "$WESTON_PID" 2>/dev/null || break
            sleep 1
        done
        if [ ! -e "$XDG_RUNTIME_DIR/$SOCKET" ]; then
            echo "::error::weston failed to create its Wayland socket" >&2
            cat "$OUT.weston"
            exit 1
        fi

        # These must be exported into the *app's* environment. Attaching them
        # to a wrapper command (as the old CI did with `printf`) silently
        # applied them to the wrong process.
        APP_ENV=(
            XDG_SESSION_TYPE=wayland
            WAYLAND_DISPLAY="$SOCKET"
            QT_QPA_PLATFORM=wayland
        )
        ;;

    offscreen)
        APP_ENV=(QT_QPA_PLATFORM=offscreen)
        ;;

    *)
        echo "::error::unknown mode '$MODE' (expected x11, wayland or offscreen)" >&2
        exit 2
        ;;
esac

${WRAPPER[@]+"${WRAPPER[@]}"} \
    timeout "$TIMEOUT_SECS" env ${APP_ENV[@]+"${APP_ENV[@]}"} \
    "$BIN" --smoke-test >"$OUT" 2>&1
RC=$?
cat "$OUT"

# ── Assertions ────────────────────────────────────────────────────────────────
FAIL=0

if [ "$RC" -eq 124 ]; then
    echo "::error::smoke test timed out after ${TIMEOUT_SECS}s"
    FAIL=1
elif [ "$RC" -ne 0 ]; then
    echo "::error::smoke test exited with status $RC"
    FAIL=1
elif ! grep -q "smoke: OK" "$OUT"; then
    echo "::error::process exited 0 but never reported 'smoke: OK'"
    FAIL=1
fi

# The app writes one log file per run; consider only logs produced by *this*
# run, so a stale log from an earlier successful run can't mask a failure.
LOG_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/UnifiedConkyControlCenter/logs"
LATEST_LOG="$(ls -1t "$LOG_DIR"/*.log 2>/dev/null | head -1)"

if [ -n "$LATEST_LOG" ] \
   && [ "$(stat -c %Y "$LATEST_LOG" 2>/dev/null || echo 0)" -ge "$START_TS" ]; then
    echo "--- log: $LATEST_LOG ---"
    if grep -q "\[CRITICAL\]" "$LATEST_LOG"; then
        echo "::error::session log contains CRITICAL entries:"
        grep "\[CRITICAL\]" "$LATEST_LOG"
        FAIL=1
    fi
    if grep -q "Smoke test threw" "$LATEST_LOG"; then
        echo "::error::smoke test logged an exception:"
        grep "Smoke test threw" "$LATEST_LOG"
        FAIL=1
    fi
else
    echo "::warning::no new log file was produced; cannot assert on log contents"
fi

if [ "$FAIL" -ne 0 ]; then
    echo "=== SMOKE TEST FAILED ==="
    exit 1
fi

echo "=== SMOKE TEST PASSED ==="
