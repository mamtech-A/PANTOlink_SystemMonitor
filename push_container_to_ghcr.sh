#!/bin/bash

# ─────────────────────────────────────────────────────────────
# Push SystemMonitor Docker image to GitHub Container Registry (GHCR)
#
# USAGE:
#   ./push_container_to_ghcr.sh
#
# Version detection priority:
#   1. Docker image tag: system-monitor:vX.Y.Z (highest version wins)
#   2. Fallback: .version file in project root
#
# Requires:
#   docker login ghcr.io (or GHCR_TOKEN)
# ─────────────────────────────────────────────────────────────

set -uo pipefail

# ─── Config ──────────────────────────────────────────────────
GITHUB_USER="amirhoseinmasoumi"
IMAGE="system-monitor"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_DIR="${PROJECT_DIR:-$(cd "$SCRIPT_DIR/.." >/dev/null 2>&1 && pwd)}"

fail() {
    echo
    echo "════════════════════════════════════════════════════════"
    echo "❌ FAILED: $1"
    shift
    for line in "$@"; do echo "   $line"; done
    echo "════════════════════════════════════════════════════════"
    exit 1
}

# ─── 1. Detect version from local Docker images ──────────────
VERSION=$(docker images --format '{{.Tag}}' "$IMAGE" 2>/dev/null \
    | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' \
    | sort -V | tail -n 1)

# fallback: .version file
if [ -z "$VERSION" ] && [ -f "$PROJECT_DIR/.version" ]; then
    VERSION="v$(tr -d ' \n' < "$PROJECT_DIR/.version")"
fi

[ -z "$VERSION" ] && fail "No version found" \
    "No ${IMAGE}:vX.Y.Z image exists and no .version file found." \
    "Build first and tag image, e.g.:" \
    "  docker tag ${IMAGE}:latest ${IMAGE}:v1.0.0"

echo "🏷️  Detected version: $VERSION"

# ─── 2. Select local image ───────────────────────────────────
LOCAL_IMAGE="${IMAGE}:${VERSION}"

if ! docker image inspect "$LOCAL_IMAGE" >/dev/null 2>&1; then
    LOCAL_IMAGE="${IMAGE}:latest"
fi

docker image inspect "$LOCAL_IMAGE" >/dev/null 2>&1 \
    || fail "Local image not found" \
    "Missing ${IMAGE}:${VERSION} and ${IMAGE}:latest"

echo "📦 Source image: $LOCAL_IMAGE"

# ─── 3. GHCR tags ────────────────────────────────────────────
GHCR_VERSION="ghcr.io/${GITHUB_USER}/${IMAGE}:${VERSION}"
GHCR_LATEST="ghcr.io/${GITHUB_USER}/${IMAGE}:latest"

# ─── 4. Login to GHCR ────────────────────────────────────────
if [ -n "${GHCR_TOKEN:-}" ]; then
    echo "🔑 Logging into GHCR with token..."
    echo "$GHCR_TOKEN" | docker login ghcr.io -u "$GITHUB_USER" --password-stdin

elif grep -q "ghcr.io" "${HOME}/.docker/config.json" 2>/dev/null; then
    echo "🔑 Using existing docker login credentials..."

else
    read -rs -p "Enter GitHub token (write:packages scope): " GHCR_TOKEN
    echo
    echo "$GHCR_TOKEN" | docker login ghcr.io -u "$GITHUB_USER" --password-stdin
fi

# ─── 5. Tag images ───────────────────────────────────────────
echo "🏷️  Tagging images..."

docker tag "$LOCAL_IMAGE" "$GHCR_VERSION"
docker tag "$LOCAL_IMAGE" "$GHCR_LATEST"

# ─── 6. Push images ──────────────────────────────────────────
echo "🚀 Pushing versioned image..."
docker push "$GHCR_VERSION"

echo "🚀 Pushing latest image..."
docker push "$GHCR_LATEST"

# ─── 7. Done ────────────────────────────────────────────────
echo
echo "🎉 Successfully pushed SystemMonitor"
echo "   $GHCR_VERSION"
echo "   $GHCR_LATEST"
echo
echo "📦 GitHub Packages:"
echo "   https://github.com/${GITHUB_USER}?tab=packages"