#!/bin/bash

# ─────────────────────────────────────────────────────────────
# Build → Run → Verify → Version → Cleanup (SystemMonitor)
#
# WHAT IT DOES:
#   1. Builds the C++ system-monitor Docker image via docker compose.
#   2. Starts the container (ZMQ telemetry service).
#   3. Watches logs for success/error signals.
#   4. On success:
#        - tags image with vX.Y.Z
#        - updates .version file
#        - removes old image
#   5. On failure:
#        - prints logs
#        - explains likely cause
#
# USAGE:
#   ./build_and_run_verify.sh
#   ./build_and_run_verify.sh v1.2.3
# ─────────────────────────────────────────────────────────────

set -uo pipefail   # we handle errors manually for better diagnostics

# ─── Config ──────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_DIR="${PROJECT_DIR:-$SCRIPT_DIR}"

SERVICE="system-monitor"
IMAGE="system-monitor"
VERSION_FILE=".version"

VERIFY_TIMEOUT=60
VERIFY_INTERVAL=2

# Expected runtime signals (adjust to your real logs)
SUCCESS_RE='started|running|ZMQ.*connected|metrics published'
ERROR_RE='Traceback|segfault|fatal|no such file|cannot open|error|FAILED'

BUILD_LOG="/tmp/system_monitor_build.log"

# ─── Helpers ────────────────────────────────────────────────
info() { echo "ℹ️  $*"; }
ok()   { echo "✅ $*"; }

fail() {
    echo
    echo "════════════════════════════════════════════════════════"
    echo "❌ FAILED: $1"
    echo "════════════════════════════════════════════════════════"
    shift
    for line in "$@"; do echo "   $line"; done
    echo "════════════════════════════════════════════════════════"
    exit 1
}

# ─── 0. Pre-flight ──────────────────────────────────────────
cd "$PROJECT_DIR" || fail "Project not found" "Expected: $PROJECT_DIR"

[ -f docker-compose.yml ] || fail "Missing docker-compose.yml"
[ -f Dockerfile ] || fail "Missing Dockerfile"

docker info >/dev/null 2>&1 || fail "Docker not running" \
    "Start: sudo systemctl start docker"

# ─── 1. Version selection ───────────────────────────────────
if [ $# -ge 1 ]; then
    NEW_VERSION="${1#v}"
elif [ -f "$VERSION_FILE" ]; then
    CUR=$(tr -d ' \n' < "$VERSION_FILE")
    IFS='.' read -r MAJ MIN PAT <<< "$CUR"
    NEW_VERSION="${MAJ:-1}.${MIN:-0}.$(( ${PAT:-0} + 1 ))"
else
    NEW_VERSION="1.0.0"
fi

info "Version: v$NEW_VERSION"

OLD_IMAGE_ID=$(docker images -q "${IMAGE}:latest" 2>/dev/null)

# ─── 2. Build ───────────────────────────────────────────────
info "Building image..."

if ! docker compose build "$SERVICE" > "$BUILD_LOG" 2>&1; then
    echo "----- build log tail -----"
    tail -n 30 "$BUILD_LOG"
    echo "--------------------------"

    EXPL=()

    if grep -qi "libzmq\|zmq" "$BUILD_LOG"; then
        EXPL+=("ZeroMQ dependency missing or not installed (libzmq3-dev).")
        EXPL+=("Fix: ensure Dockerfile installs libzmq3-dev.")
    fi

    if grep -qi "CMake Error" "$BUILD_LOG"; then
        EXPL+=("CMake configuration failed (missing source or wrong paths).")
    fi

    if grep -qi "no space left" "$BUILD_LOG"; then
        EXPL+=("Disk full. Run: docker system prune -a")
    fi

    fail "Build failed" "${EXPL[@]}"
fi

NEW_IMAGE_ID=$(docker images -q "${IMAGE}:latest")
ok "Build OK: ${NEW_IMAGE_ID}"

# ─── 3. Run ────────────────────────────────────────────────
info "Starting container..."

docker compose down >/dev/null 2>&1
docker compose up -d --force-recreate "$SERVICE" >/dev/null 2>&1 \
    || fail "Container failed to start" \
    "Check: docker compose logs $SERVICE"

# ─── 4. Verify logs ─────────────────────────────────────────
info "Verifying runtime behavior..."

ELAPSED=0
RESULT="timeout"

while [ "$ELAPSED" -lt "$VERIFY_TIMEOUT" ]; do
    LOGS=$(docker compose logs --no-color --tail=200 "$SERVICE" 2>/dev/null)

    if echo "$LOGS" | grep -qE "$SUCCESS_RE"; then
        RESULT="success"
        break
    fi

    if echo "$LOGS" | grep -qE "$ERROR_RE"; then
        RESULT="error"
        break
    fi

    sleep "$VERIFY_INTERVAL"
    ELAPSED=$((ELAPSED + VERIFY_INTERVAL))
done

echo

# ─── 5. Handle result ───────────────────────────────────────
if [ "$RESULT" != "success" ]; then
    echo "----- logs -----"
    docker compose logs --tail=80 "$SERVICE"
    echo "-----------------"

    EXPL=()

    case "$RESULT" in
        error)
            if echo "$LOGS" | grep -qi "zmq"; then
                EXPL+=("ZeroMQ connection failure (wrong endpoint or host/port mismatch).")
                EXPL+=("Check ZMQClient configuration: tcp://localhost:xxxx vs host networking.")
            elif echo "$LOGS" | grep -qi "segfault"; then
                EXPL+=("Crash in native C++ code (likely invalid memory access in monitor modules).")
            else
                EXPL+=("Application logged a runtime error before successful startup.")
            fi
            ;;
        timeout)
            EXPL+=("No startup signal detected within timeout.")
            EXPL+=("If using systemd or delayed init, increase VERIFY_TIMEOUT.")
            ;;
    esac

    fail "Verification failed ($RESULT)" "${EXPL[@]}"
fi

ok "SystemMonitor started successfully"

# ─── 6. Tag version ─────────────────────────────────────────
docker tag "${IMAGE}:latest" "${IMAGE}:v${NEW_VERSION}"
echo "$NEW_VERSION" > "$VERSION_FILE"

ok "Tagged v$NEW_VERSION"

# ─── 7. Cleanup old image ───────────────────────────────────
if [ -n "$OLD_IMAGE_ID" ]; then
    info "Cleaning old image..."
    docker rmi -f "$OLD_IMAGE_ID" >/dev/null 2>&1 || true
fi

docker image prune -f >/dev/null 2>&1

# ─── Done ───────────────────────────────────────────────────
echo
echo "🎉 DONE"
echo "   Image: ${IMAGE}:latest (v${NEW_VERSION})"
echo "   Logs : docker compose logs -f $SERVICE"
echo "   Stop : docker compose down"