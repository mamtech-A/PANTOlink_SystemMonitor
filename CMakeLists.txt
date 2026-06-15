cmake_minimum_required(VERSION 3.10)
project(SystemMonitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

add_subdirectory(source)

add_executable(system_monitor main.cpp)

target_link_libraries(system_monitor PRIVATE
    system_monitor_lib
    Threads::Threads
    ${ZMQ_LIBRARIES}
)

target_include_directories(system_monitor PRIVATE
    ${ZMQ_INCLUDE_DIRS}
)
