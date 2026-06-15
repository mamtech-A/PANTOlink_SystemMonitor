#!/bin/bash

set -uo pipefail

# ─── Config ────────────────────────────────────────────────
GITHUB_USER="amirhoseinmasoumi"
IMAGE="system-monitor"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." >/dev/null 2>&1 && pwd)"

fail() {
    echo
    echo "❌ FAILED: $1"
    shift
    for l in "$@"; do echo "   $l"; done
    exit 1
}

# ─── Version detect ────────────────────────────────────────
VERSION=$(docker images --format '{{.Tag}}' "$IMAGE" \
    | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' \
    | sort -V | tail -n 1)

if [ -z "$VERSION" ] && [ -f "$PROJECT_DIR/.version" ]; then
    VERSION="v$(cat "$PROJECT_DIR/.version")"
fi

[ -z "$VERSION" ] && fail "No version found"

echo "🏷️ Detected version: $VERSION"

# ─── Source image ───────────────────────────────────────────
LOCAL_IMAGE="${IMAGE}:${VERSION}"

docker image inspect "$LOCAL_IMAGE" >/dev/null 2>&1 \
    || fail "Missing local image $LOCAL_IMAGE"

echo "📦 Source: $LOCAL_IMAGE"

# ─── GHCR login check ───────────────────────────────────────
if ! docker info | grep -q "Username: amirhoseinmasoumi"; then
    echo "🔑 Logging into GHCR..."
    echo "$GHCR_TOKEN" | docker login ghcr.io -u "$GITHUB_USER" --password-stdin \
        || fail "GHCR login failed"
fi

# ─── Tags ───────────────────────────────────────────────────
GHCR_VERSION="ghcr.io/${GITHUB_USER}/${IMAGE}:${VERSION}"
GHCR_LATEST="ghcr.io/${GITHUB_USER}/${IMAGE}:latest"

echo "🏷️ Tagging..."

docker tag "$LOCAL_IMAGE" "$GHCR_VERSION"
docker tag "$LOCAL_IMAGE" "$GHCR_LATEST"

# ─── PUSH VERSION ───────────────────────────────────────────
echo "🚀 Pushing version..."
docker push "$GHCR_VERSION" || fail "Push failed (version tag)"

# ─── PUSH LATEST ────────────────────────────────────────────
echo "🚀 Pushing latest..."
docker push "$GHCR_LATEST" || fail "Push failed (latest tag)"

# ─── DONE ───────────────────────────────────────────────────
echo
echo "🎉 SUCCESS"
echo "   $GHCR_VERSION"
echo "   $GHCR_LATEST"