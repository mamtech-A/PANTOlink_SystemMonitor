#!/bin/bash

# ──────────────────────────────────────────────
# USAGE: ./build_go_docker.sh <container_name> <version> "<description>"
# EXAMPLE: ./build_go_docker.sh manager v1.0.0 "Initial stable version"
# ──────────────────────────────────────────────

if [ $# -lt 2 ]; then
    echo "Usage: $0 <container_name> <version> [description]"
    exit 1
fi

# ─── Input parameters ─────────────────────────
CONTAINER_NAME=$(echo "$1" | tr '[:upper:]' '[:lower:]')
VERSION="$2"
DESCRIPTION="${3:-No description provided.}"

# ─── Image info ───────────────────────────────
GITHUB_USER="amirhoseinmasoumi"
GHCR_IMAGE="ghcr.io/${GITHUB_USER}/${CONTAINER_NAME}:${VERSION}"
GO_BINARY_NAME="${CONTAINER_NAME}"

# ─── Paths ────────────────────────────────────
BUILD_DIR="./build"
BUILD_OUTPUT="${BUILD_DIR}/${GO_BINARY_NAME}"

# ─── Check for libzmq (for pebbe/zmq4) ────────
if ! dpkg -s libzmq3-dev >/dev/null 2>&1; then
    echo "📦 Installing libzmq3-dev (required for ZeroMQ)..."
    sudo apt update && sudo apt install -y libzmq3-dev
fi

# ─── Build binary ─────────────────────────────
echo "🔨 Building Go binary for native Pi architecture (CGO enabled)"
mkdir -p "$BUILD_DIR"

CGO_ENABLED=1 go build -o "$BUILD_OUTPUT" ./main.go

if [ $? -ne 0 ]; then
    echo "❌ Go build failed. Make sure C libs and CGO setup are working."
    exit 1
fi

echo "✅ Go binary built at: $BUILD_OUTPUT"

# ─── Generate Dockerfile ──────────────────────
echo "🛠️  Writing Dockerfile..."
cat > "${BUILD_DIR}/Dockerfile" <<EOF
FROM ubuntu:24.04

LABEL maintainer="${GITHUB_USER}"
LABEL version="${VERSION}"
LABEL description="${DESCRIPTION}"

LABEL org.opencontainers.image.title="${CONTAINER_NAME}"
LABEL org.opencontainers.image.version="${VERSION}"
LABEL org.opencontainers.image.description="${DESCRIPTION}"
LABEL org.opencontainers.image.authors="${GITHUB_USER}"

RUN apt update && apt install -y libzmq5 && rm -rf /var/lib/apt/lists/*

COPY ${GO_BINARY_NAME} /${GO_BINARY_NAME}

ENTRYPOINT ["/${GO_BINARY_NAME}"]
EOF

# ─── Build Docker image ───────────────────────
echo "🐳 Building Docker image: $GHCR_IMAGE"
docker build --network=host -t "$GHCR_IMAGE" "$BUILD_DIR"

if [ $? -ne 0 ]; then
    echo "❌ Docker build failed."
    exit 1
fi

echo "✅ Docker image built."

# ─── Push to GHCR ─────────────────────────────
echo "🚀 Pushing image to GHCR: $GHCR_IMAGE"
docker push "$GHCR_IMAGE"

if [ $? -eq 0 ]; then
    echo "✅ Pushed successfully: $GHCR_IMAGE"
else
    echo "❌ Push failed. Run:"
    echo "   echo <TOKEN> | docker login ghcr.io -u ${GITHUB_USER} --password-stdin"
    exit 1
fi

echo "🎉 Done! Version: $VERSION | Description: $DESCRIPTION"
