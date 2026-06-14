cmake_minimum_required(VERSION 3.10)
project(SystemMetric VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find required packages
find_package(Threads REQUIRED)

# Define the executable
add_executable(system_metric
main.cpp
cpuMonitor.cpp
internetConnectionMonitor.cpp
diskMonitor.cpp
RAMMonitor.cpp
ZMQClient.cpp
jsonMaker.cpp)

# Link libraries
target_link_libraries(system_metric PRIVATE Threads::Threads zmq
 ${CMAKE_CURRENT_SOURCE_DIR})

# Set compile options
target_compile_options(system_metric PRIVATE 
    -Wall 
    -Wextra 
    -Wpedantic
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-g -O0 -DDEBUG>
)

# Installation rules
install(TARGETS system_metric RUNTIME DESTINATION bin)