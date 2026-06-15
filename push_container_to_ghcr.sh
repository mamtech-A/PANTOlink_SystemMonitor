#!/bin/bash

# ─────────────────────────────────────────────────────────────
# Push the system_monitor image to GHCR, reading the VERSION from the
# image's own version tag (e.g. system_monitor:v1.0.3) — no arguments.
#
# USAGE:   ./push_container_to_ghcr.sh
#
# The version is detected like this, in order:
#   1. Highest  system_monitor:vX.Y.Z  tag among the local images.
#   2. Failing that, the .version file in the project folder (../).
# Both are produced by build_run_verify.sh, so a normal build → push works.
#
# AUTH: set a token with the `write:packages` scope once:
#         export GHCR_TOKEN=ghp_xxxxxxxxxxxxxxxx
#       If unset and you're not already logged in, it prompts (hidden input).
# ─────────────────────────────────────────────────────────────

set -uo pipefail

# ─── Config ──────────────────────────────────────────────────
GITHUB_USER="amirhoseinmasoumi"
IMAGE="system_monitor"          # local image name AND GHCR package name

# Project folder (parent of this script) — used for the .version fallback.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_DIR="${PROJECT_DIR:-$(cd "$SCRIPT_DIR/.." >/dev/null 2>&1 && pwd)}"

fail() {
    local title="$1"; shift
    echo
    echo "════════════════════════════════════════════════════════"
    echo "❌ FAILED: $title"
    echo "════════════════════════════════════════════════════════"
    for line in "$@"; do echo "   $line"; done
    echo "════════════════════════════════════════════════════════"
    exit 1
}

# ─── 1. Read the version from the image tags ─────────────────
VERSION=$(docker images --format '{{.Tag}}' "$IMAGE" 2>/dev/null \
            | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' \
            | sort -V | tail -n 1)

# Fallback: the .version file written by build_run_verify.sh
if [ -z "$VERSION" ] && [ -f "$PROJECT_DIR/.version" ]; then
    VERSION="v$(tr -d ' \n' < "$PROJECT_DIR/.version")"
fi

[ -z "$VERSION" ] && fail "No version found" \
    "No ${IMAGE}:vX.Y.Z image tag exists and no $PROJECT_DIR/.version file." \
    "Version it first (build_run_verify.sh) or tag it manually:" \
    "  docker tag ${IMAGE}:latest ${IMAGE}:v1.0.0"

echo "🏷️  Detected version: $VERSION"

# ─── 2. Pick the local source image (prefer the exact version tag) ──
LOCAL_IMAGE="${IMAGE}:${VERSION}"
if ! docker image inspect "$LOCAL_IMAGE" >/dev/null 2>&1; then
    LOCAL_IMAGE="${IMAGE}:latest"
fi
docker image inspect "$LOCAL_IMAGE" >/dev/null 2>&1 \
    || fail "Local image not found" "Neither ${IMAGE}:${VERSION} nor ${IMAGE}:latest exists. Build it first."
echo "📦 Source image: $LOCAL_IMAGE  ($(docker image inspect "$LOCAL_IMAGE" --format '{{.Architecture}}'))"

GHCR_VERSION="ghcr.io/${GITHUB_USER}/${IMAGE}:${VERSION}"
GHCR_LATEST="ghcr.io/${GITHUB_USER}/${IMAGE}:latest"

# ─── 3. Log in to GHCR ───────────────────────────────────────
if [ -n "${GHCR_TOKEN:-}" ]; then
    echo "🔑 Logging in to ghcr.io as $GITHUB_USER ..."
    echo "$GHCR_TOKEN" | docker login ghcr.io -u "$GITHUB_USER" --password-stdin
elif grep -q 'ghcr.io' "${HOME}/.docker/config.json" 2>/dev/null; then
    echo "🔑 Using stored ghcr.io credentials."
else
    read -rs -p "Enter GitHub token (write:packages scope): " GHCR_TOKEN; echo
    echo "$GHCR_TOKEN" | docker login ghcr.io -u "$GITHUB_USER" --password-stdin
fi

# ─── 4. Tag and push (version + latest) ──────────────────────
echo "🏷️  Tagging $LOCAL_IMAGE -> $GHCR_VERSION  and  :latest"
docker tag "$LOCAL_IMAGE" "$GHCR_VERSION"
docker tag "$LOCAL_IMAGE" "$GHCR_LATEST"

echo "🚀 Pushing $GHCR_VERSION ..."
docker push "$GHCR_VERSION"
echo "🚀 Pushing $GHCR_LATEST ..."
docker push "$GHCR_LATEST"

echo
echo "🎉 Done!  Pushed $VERSION"
echo "   $GHCR_VERSION"
echo "   $GHCR_LATEST"
echo "   Packages: https://github.com/${GITHUB_USER}?tab=packages"