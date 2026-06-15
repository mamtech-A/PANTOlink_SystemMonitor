#!/bin/bash

set -euo pipefail

# ─────────────────────────────────────────────────────────────
# GHCR Push Script (robust version, ECR-style reliability)
# ─────────────────────────────────────────────────────────────

GITHUB_USER="amirhoseinmasoumi"
IMAGE="system-monitor"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." >/dev/null 2>&1 && pwd)"

fail() {
    echo
    echo "❌ FAILED: $1"
    shift
    for line in "$@"; do echo "   $line"; done
    exit 1
}

# ─────────────────────────────────────────────────────────────
# 1. Detect version (same logic as before but safer)
# ─────────────────────────────────────────────────────────────

VERSION=$(docker images --format '{{.Tag}}' "$IMAGE" \
    | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' \
    | sort -V | tail -n 1 || true)

if [ -z "$VERSION" ] && [ -f "$PROJECT_DIR/.version" ]; then
    VERSION="v$(cat "$PROJECT_DIR/.version")"
fi

[ -z "$VERSION" ] && fail "No version found for image"

echo "🏷️ Detected version: $VERSION"

LOCAL_IMAGE="${IMAGE}:${VERSION}"

docker image inspect "$LOCAL_IMAGE" >/dev/null 2>&1 \
    || fail "Local image not found: $LOCAL_IMAGE"

echo "📦 Source image: $LOCAL_IMAGE"

# ─────────────────────────────────────────────────────────────
# 2. ALWAYS LOGIN (FIX FOR YOUR ERROR)
# ─────────────────────────────────────────────────────────────

echo "🔑 Logging into GHCR..."

if [ -z "${GHCR_TOKEN:-}" ]; then
    docker login ghcr.io -u "$GITHUB_USER"
else
    echo "$GHCR_TOKEN" | docker login ghcr.io -u "$GITHUB_USER" --password-stdin
fi

# ─────────────────────────────────────────────────────────────
# 3. Tag images
# ─────────────────────────────────────────────────────────────

GHCR_VERSION="ghcr.io/${GITHUB_USER}/${IMAGE}:${VERSION}"
GHCR_LATEST="ghcr.io/${GITHUB_USER}/${IMAGE}:latest"

echo "🏷️ Tagging images..."

docker tag "$LOCAL_IMAGE" "$GHCR_VERSION"
docker tag "$LOCAL_IMAGE" "$GHCR_LATEST"

# ─────────────────────────────────────────────────────────────
# 4. PUSH (fail-safe like your ECR script)
# ─────────────────────────────────────────────────────────────

echo "🚀 Pushing version..."
docker push "$GHCR_VERSION" || fail "Failed pushing version image"

echo "🚀 Pushing latest..."
docker push "$GHCR_LATEST" || fail "Failed pushing latest image"

# ─────────────────────────────────────────────────────────────
# 5. SUCCESS
# ─────────────────────────────────────────────────────────────

echo
echo "============================================================"
echo "🎉 SUCCESSFULLY PUSHED"
echo "============================================================"
echo "  Version: $GHCR_VERSION"
echo "  Latest : $GHCR_LATEST"
echo "============================================================"