# SystemMonitor — C++ ZMQ metrics container
# Base: Ubuntu 22.04 (ARM64 compatible for Jetson or x86 if rebuilt)

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# =========================
# Build dependencies
# =========================
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    libzmq3-dev \
    libzmq5 \
    && rm -rf /var/lib/apt/lists/*

# =========================
# App directory
# =========================
WORKDIR /app

# =========================
# Copy project
# =========================
COPY . /app

# =========================
# Build system
# =========================
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# =========================
# Runtime
# =========================
WORKDIR /app/build/bin

# Expose nothing (ZMQ / UDP typically internal)
# EXPOSE not required unless you later add TCP server

# Default executable
CMD ["./system_monitor"]