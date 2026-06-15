#!/bin/bash

# ─────────────────────────────────────────────────────────────
# SystemMonitor Build & Run Script (CLEAN VERSION)
#
# Rules:
#   - If user provides version → use vX.Y.Z
#   - If not → use latest
#   - Compose uses IMAGE_TAG only
# ─────────────────────────────────────────────────────────────

set -uo pipefail

# ─── Config ──────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_DIR="${PROJECT_DIR:-$SCRIPT_DIR}"

SERVICE="system_monitor"
IMAGE="system_monitor"

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

# ─── 0. Setup ───────────────────────────────────────────────
cd "$PROJECT_DIR" || fail "Project not found"

[ -f docker-compose.yml ] || fail "Missing docker-compose.yml"
[ -f Dockerfile ] || fail "Missing Dockerfile"

docker info >/dev/null 2>&1 || fail "Docker not running"

# ─── 1. VERSION LOGIC ────────────────────────────────────────
if [ $# -ge 1 ]; then
    IMAGE_TAG="v${1#v}"
else
    IMAGE_TAG="latest"
fi

info "Using image tag: $IMAGE_TAG"

export IMAGE_TAG

# ─── 2. BUILD ────────────────────────────────────────────────
info "Building image..."

if ! docker compose build "$SERVICE"; then
    fail "Build failed" "Check Dockerfile / dependencies / CMake"
fi

ok "Build completed"

# ─── 3. RUN ──────────────────────────────────────────────────
info "Starting container..."

docker compose down >/dev/null 2>&1

if ! docker compose up -d "$SERVICE"; then
    docker compose logs --tail=80 "$SERVICE"
    fail "Container failed to start"
fi

ok "Container started"

# ─── 4. BASIC CHECK ──────────────────────────────────────────
sleep 2

STATE=$(docker inspect -f '{{.State.Status}}' system_monitor 2>/dev/null || echo "missing")

if [ "$STATE" != "running" ]; then
    docker compose logs --tail=80 "$SERVICE"
    fail "Container not running (state=$STATE)"
fi

ok "Container is running"

# ─── 5. DONE ────────────────────────────────────────────────
echo
echo "🎉 DONE"
echo "   Image   : system_monitor:${IMAGE_TAG}"
echo "   Logs    : docker compose logs -f $SERVICE"
echo "   Stop    : docker compose down"