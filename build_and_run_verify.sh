#!/bin/bash

# ─────────────────────────────────────────────────────────────
# Build → Run → Version → Cleanup (SystemMonitor)
#
# SIMPLIFIED VERSION:
#   ❌ No log-based startup verification
#   ✔ Only ensures container starts successfully
# ─────────────────────────────────────────────────────────────

set -uo pipefail

# ─── Config ──────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_DIR="${PROJECT_DIR:-$SCRIPT_DIR}"

SERVICE="system-monitor"
IMAGE="system-monitor"
VERSION_FILE=".version"

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

docker info >/dev/null 2>&1 || fail "Docker not running"

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
    fail "Build failed" "Check Dockerfile or dependencies (CMake / ZMQ / lib issues)"
fi

NEW_IMAGE_ID=$(docker images -q "${IMAGE}:latest")
ok "Build OK: ${NEW_IMAGE_ID}"

# ─── 3. Run ────────────────────────────────────────────────
info "Starting container..."

docker compose down >/dev/null 2>&1

if ! docker compose up -d --force-recreate "$SERVICE" >/dev/null 2>&1; then
    docker compose logs --tail=50 "$SERVICE"
    fail "Container failed to start" "Check docker-compose configuration or runtime crash"
fi

ok "Container started successfully"

# ─── 4. Basic running check (no log parsing) ────────────────
sleep 3

STATE=$(docker inspect -f '{{.State.Status}}' "$SERVICE" 2>/dev/null || echo "missing")

if [ "$STATE" != "running" ]; then
    docker compose logs --tail=80 "$SERVICE"
    fail "Container is not running" "State: $STATE"
fi

ok "Container is running"

# ─── 5. Versioning ──────────────────────────────────────────
docker tag "${IMAGE}:latest" "${IMAGE}:v${NEW_VERSION}"
echo "$NEW_VERSION" > "$VERSION_FILE"

ok "Tagged version v$NEW_VERSION"

# ─── 6. Cleanup old image ───────────────────────────────────
if [ -n "$OLD_IMAGE_ID" ]; then
    info "Cleaning old image..."
    docker rmi -f "$OLD_IMAGE_ID" >/dev/null 2>&1 || true
fi

docker image prune -f >/dev/null 2>&1

# ─── Done ───────────────────────────────────────────────────
echo
echo "🎉 DONE"
echo "   Image : ${IMAGE}:v${NEW_VERSION}"
echo "   Logs  : docker compose logs -f $SERVICE"
echo "   Stop  : docker compose down"