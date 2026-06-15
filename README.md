# SystemMonitor

A comprehensive system monitoring application that collects and reports detailed metrics about CPU, RAM, disk usage, internet connectivity, and equipment status. Built with C++ and designed for continuous monitoring and reliable data logging with cloud synchronization.

## Overview

SystemMonitor is a robust monitoring application that continuously tracks system performance metrics and device health. It provides real-time monitoring capabilities with automatic data persistence, offline support, and scheduled cloud synchronization to AWS S3.

## Features

### Core Monitoring Capabilities

- **CPU Monitoring**
  - Total CPU usage percentage
  - Per-core CPU usage breakdown
  - Load average (1, 5, 15 minute averages)
  - CPU temperature readings
  - Top 10 processes by CPU consumption

- **Memory (RAM) Monitoring**
  - Total RAM usage percentage
  - Top 10 processes by memory consumption
  - Available vs. used memory tracking

- **Disk Monitoring**
  - Disk capacity and utilization
  - Top 5 heavy files on the system
  - Real-time disk space alerts

- **Network Monitoring**
  - Internet connectivity status
  - Connection reliability checks

- **System Information**
  - Boot time tracking
  - Equipment status (Cameras, Panto pack, Panto receiver)

### Data Management

- **Continuous Logging**: Collects metrics every minute automatically
- **Database Persistence**: Stores all collected data in a local database
- **Cloud Synchronization**: Uploads previous day's data to AWS S3 daily at 12:00 AM
- **Offline Support**: Caches data locally when internet is unavailable; syncs all logs upon reconnection

## Requirements

### System Dependencies

- **C++17 or later** compatible compiler (GCC, Clang)
- **CMake 3.10** or later
- **ZeroMQ (libzmq)**: For inter-process communication
- **POSIX-compliant operating system** (Linux, macOS, or Windows with WSL)

### Build Dependencies

```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake libzmq3-dev

# macOS
brew install cmake zeromq

# Fedora/RedHat
sudo dnf install gcc gcc-c++ cmake zeromq-devel
```

### Docker Requirements (Optional)

- **Docker**: 20.10 or later
- **Docker Compose**: 1.29 or later

## Installation & Building

### Clone the Repository

```bash
cd /path/to/PANTOlink
git clone <repository-url> SystemMonitor
cd SystemMonitor
```

### Build from Source

```bash
# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build the project
cmake --build .

# The executable will be available at: build/bin/system_monitor
```

### Build with custom options

```bash
# Clean rebuild
cd build
cmake --build . --clean-first

# Build with verbose output
cmake --build . --verbose
```

### Build with Docker

#### Using Docker Compose (Recommended)

```bash
# Build and run the container
docker-compose up --build

# Run in detached mode
docker-compose up -d --build

# Stop the service
docker-compose down

# View logs
docker-compose logs -f system-monitor
```

#### Using Docker directly

```bash
# Build the image
docker build -t system-monitor:latest .

# Run the container
docker run -d \
  --name system-monitor \
  --network host \
  --restart unless-stopped \
  -v ./logs:/app/logs \
  system-monitor:latest

# View logs
docker logs -f system-monitor

# Stop the container
docker stop system-monitor
```

#### Build and Verify with Script

```bash
# Automated build, run, and verification
./build_and_run_verify.sh

# Specify a version tag
./build_and_run_verify.sh v1.2.3
```

The script will:
1. Build the Docker image using docker-compose
2. Start the container
3. Monitor logs for success/error signals
4. Tag the image with version number on success
5. Update the `.version` file
6. Clean up old images

#### Push to GitHub Container Registry

```bash
# Authenticate with GHCR (one-time setup)
docker login ghcr.io

# Push the image to GHCR
./push_container_to_ghcr.sh
```

The script will:
1. Detect version from existing Docker image tags or `.version` file
2. Tag the image for GHCR
3. Push to `ghcr.io/amirhoseinmasoumi/system-monitor:vX.Y.Z`

## Usage

### Running the Application

```bash
./build/bin/system_monitor
```

The application will:
1. Initialize all monitoring modules
2. Connect to the monitoring server via ZMQ at `tcp://localhost:5555`
3. Begin collecting metrics every minute
4. Store data locally in the database
5. Handle automatic cloud uploads according to the schedule

### Configuration

The default server address is configured as:
```
tcp://localhost:5555
```

To modify the server address, edit the `serverAddress` variable in [main.cpp](main.cpp#L17).

## Project Architecture

### Core Components

| Component | File | Purpose |
|-----------|------|---------|
| CPU Monitor | `source/cpuMonitor.cpp` | Tracks CPU usage and temperature |
| RAM Monitor | `source/RAMMonitor.cpp` | Monitors memory utilization |
| Disk Monitor | `source/diskMonitor.cpp` | Tracks disk space and file sizes |
| Internet Monitor | `source/internetConnectionMonitor.cpp` | Checks network connectivity |
| Boot Time Monitor | `source/bootTimeMonitor.hpp` | Records system boot time |
| ZMQ Client | `source/ZMQClient.cpp` | Handles server communication and metrics transmission |
| JSON Maker | `source/jsonMaker.cpp` | Serializes metrics data to JSON format |

### Directory Structure

```
SystemMonitor/
├── CMakeLists.txt              # CMake configuration
├── main.cpp                    # Application entry point
├── README.md                   # This file
├── source/                     # Source code directory
│   ├── CMakeLists.txt
│   ├── include/               # Header files
│   │   ├── bootTimeMonitor.hpp
│   │   ├── cpuMonitor.hpp
│   │   ├── diskMonitor.hpp
│   │   ├── internetConnectionMonitor.hpp
│   │   ├── jsonMaker.hpp
│   │   ├── RAMMonitor.hpp
│   │   └── ZMQClient.hpp
│   ├── bootTimeMonitor.cpp
│   ├── cpuMonitor.cpp
│   ├── diskMonitor.cpp
│   ├── internetConnectionMonitor.cpp
│   ├── jsonMaker.cpp
│   ├── RAMMonitor.cpp
│   └── ZMQClient.cpp
└── build/                     # Build output directory
    ├── bin/
    │   └── system_monitor    # Compiled executable
    └── ...
```

## Error Handling

The application includes comprehensive error handling:
- Graceful shutdown on fatal errors
- Standard exception catching with detailed error messages
- Fallback mechanisms for network failures

If the application encounters a fatal error, it will:
1. Log the error message to stderr
2. Exit with return code 1
3. Preserve cached data for later synchronization

## Monitoring Metrics Details

### Collection Interval
- **Frequency**: Every 1 minute
- **Storage**: Local database and JSON format
- **Transmission**: Via ZMQ to monitoring server

### Data Points per Interval

**CPU Metrics**
- Total usage percentage
- Individual core usage
- 1, 5, 15-minute load averages
- Temperature in Celsius
- Top 10 process details (PID, name, usage)

**RAM Metrics**
- Total usage percentage
- Used/Available breakdown
- Top 10 process details (PID, name, memory)

**Disk Metrics**
- Total capacity
- Used/Available space
- Percentage utilization
- Top 5 largest files with paths

**Network Metrics**
- Connectivity status (online/offline)
- Last successful connection timestamp

**System Metrics**
- System boot time
- Equipment status checks

## Cloud Synchronization

### S3 Upload Schedule
- **Time**: 12:00 AM (midnight) daily
- **Data**: All metrics from the previous 24-hour period
- **Behavior**: 
  - Uploads only when internet is available
  - Queues data if offline
  - Automatically syncs all pending logs when connection restored

### Offline Support
- Stores all metrics locally during connectivity loss
- Maintains data integrity and timestamp information
- Automatically resuming uploads without data loss

## Development

### Adding New Metrics

1. Create a new monitor class in `source/`
2. Add header file to `source/include/`
3. Include in `main.cpp`
4. Instantiate in the main monitoring loop
5. Update `ZMQClient` to transmit new metrics

### Building in Debug Mode

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

## Docker & Containerization

### Docker Architecture

The application is containerized using Docker for easy deployment and portability across different environments (x86, ARM64, etc.).

#### Dockerfile

- **Base Image**: Ubuntu 22.04 (ARM64 compatible for Jetson devices)
- **Build**: Multi-stage build with CMake and Release mode optimization
- **Runtime**: Minimal footprint with only required libraries
- **Entry Point**: Automatically runs `system_monitor` on container start

#### Docker Compose

The `docker-compose.yml` provides:
- Automatic image building from Dockerfile
- Host networking mode for ZMQ communication
- Auto-restart policy (unless manually stopped)
- Volume mapping for logs persistence
- Simplified lifecycle management (up/down/logs)

### Docker Usage Examples

**Production Deployment**
```bash
# Start service with auto-restart
docker-compose up -d --build

# Monitor logs in real-time
docker-compose logs -f system-monitor

# Graceful shutdown
docker-compose down
```

**Development & Debugging**
```bash
# Run with interactive terminal
docker-compose run --rm system-monitor bash

# Execute command in running container
docker exec system-monitor ./system_monitor --version
```

**Custom Network**
```bash
# Override network mode for different deployment scenarios
docker run --network bridge -p 5555:5555 system-monitor:latest
```

## Scripts

The project includes helper scripts for automation and deployment workflows.

### build_and_run_verify.sh

**Purpose**: Automated build, run, and verification pipeline with version management

**Features**:
- Builds Docker image via docker-compose
- Starts the container and monitors for startup signals
- Validates successful startup by parsing logs
- Auto-tags image with semantic versioning (v1.2.3)
- Updates `.version` file on successful build
- Cleans up old image versions
- Provides detailed error diagnostics on failure

**Usage**:
```bash
# Build with auto-detected version
./build_and_run_verify.sh

# Build with specific version
./build_and_run_verify.sh v1.2.3

# Check exit code
echo $?  # 0 = success, 1 = failure
```

**Configuration** (edit script as needed):
- `VERIFY_TIMEOUT`: How long to wait for startup signals (default: 60s)
- `SUCCESS_RE`: Regex pattern to match success logs
- `ERROR_RE`: Regex pattern to match error logs

### push_container_to_ghcr.sh

**Purpose**: Automated container image push to GitHub Container Registry (GHCR)

**Features**:
- Auto-detects version from Docker image tags or `.version` file
- Validates Docker authentication with GHCR
- Tags image with proper GHCR naming convention
- Pushes to `ghcr.io/amirhoseinmasoumi/system-monitor:vX.Y.Z`
- Clear error messages and troubleshooting hints

**Prerequisites**:
```bash
# Authenticate with GHCR (use personal access token with write:packages scope)
docker login ghcr.io
# Username: <github-username>
# Password: <personal-access-token>
```

**Usage**:
```bash
# Push with auto-detected version
./push_container_to_ghcr.sh

# Manually set version if needed
docker tag system-monitor:latest system-monitor:v2.0.0
./push_container_to_ghcr.sh

# Verify push
docker pull ghcr.io/amirhoseinmasoumi/system-monitor:v2.0.0
```

**Configuration**:
- `GITHUB_USER`: Repository owner (default: amirhoseinmasoumi)
- `IMAGE`: Docker image name (default: system-monitor)

### Version Management

Version tagging follows semantic versioning (vX.Y.Z):

1. **Build Script** (`build_and_run_verify.sh`):
   - Tags successful builds automatically
   - Stores version in `.version` file

2. **Push Script** (`push_container_to_ghcr.sh`):
   - Reads version from Docker tags or `.version` file
   - Maintains version consistency across GHCR

3. **Manual Version Update**:
   ```bash
   # Create .version file
   echo "1.2.3" > .version
   
   # Tag Docker image
   docker tag system-monitor:latest system-monitor:v1.2.3
   ```

## Troubleshooting

### Native Build Issues

**ZMQ Connection Failed**
- Verify the server is running at `tcp://localhost:5555`
- Check firewall rules for port 5555
- Ensure libzmq is properly installed

**Permission Denied on build**
```bash
chmod +x build/bin/system_monitor
```

**CMake Configuration Issues**
```bash
rm -rf build
mkdir build
cd build
cmake ..
```

### Docker Issues

**Docker daemon not running**
```bash
# Start Docker daemon
sudo systemctl start docker

# Or on macOS with Docker Desktop
# Simply open the Docker Desktop application
```

**docker-compose: command not found**
```bash
# Install docker-compose
sudo apt-get install docker-compose    # Ubuntu/Debian
brew install docker-compose             # macOS
```

**Build fails with "Permission denied"**
```bash
# Ensure user is in docker group
sudo usermod -aG docker $USER
newgrp docker

# Or use sudo
sudo docker-compose up --build
```

**Container exits immediately**
```bash
# Check logs for errors
docker logs system-monitor

# View detailed build output
docker-compose up --build (without -d)
```

**Port/Network issues with Docker**
```bash
# Check if service is listening on correct port
netstat -tlnp | grep 5555

# If host networking fails, try bridge network
docker run --network bridge -p 5555:5555 system-monitor:latest
```

**Build script (`build_and_run_verify.sh`) fails**

Check the build log:
```bash
cat /tmp/system_monitor_build.log
```

Common causes:
- Missing dependencies in Dockerfile
- CMake configuration errors
- ZMQ library issues in container
- Insufficient disk space

**GHCR push authentication failed**

Re-authenticate with GHCR:
```bash
# Clear old authentication
docker logout ghcr.io

# Login with personal access token
docker login ghcr.io
# Username: <github-username>
# Password: <PAT with write:packages scope>

# Retry push
./push_container_to_ghcr.sh
```

### Script Issues

**build_and_run_verify.sh: Permission denied**
```bash
chmod +x build_and_run_verify.sh
chmod +x push_container_to_ghcr.sh
```

**Scripts not finding Docker**
```bash
# Ensure Docker is installed and in PATH
which docker
which docker-compose

# Add to PATH if needed
export PATH="/usr/bin:$PATH"
```

**Version detection not working**
```bash
# Manual version setup
echo "1.0.0" > .version
docker tag system-monitor:latest system-monitor:v1.0.0
./push_container_to_ghcr.sh
```

## Dependencies License

- **ZeroMQ (libzmq)**: Licensed under LGPL v3
- **CMake**: Licensed under BSD 3-Clause
- **C++ Standard Library**: Per compiler license

## Support & Contribution

For issues, feature requests, or contributions, please refer to the PANTOlink project guidelines.

## License

See the LICENSE file in the PANTOlink repository for licensing information.

---

**Last Updated**: 2026-06-15  
**Project**: SystemMonitor v1.0  
**Status**: Active Development