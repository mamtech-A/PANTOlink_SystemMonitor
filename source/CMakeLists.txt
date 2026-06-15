file(GLOB SYSTEM_MONITOR_SOURCES
    *.cpp
)

add_library(system_monitor_lib STATIC
    ${SYSTEM_MONITOR_SOURCES}
)

target_include_directories(system_monitor_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(system_monitor_lib PRIVATE
    ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries(system_monitor_lib PUBLIC
    Threads::Threads
    ${ZMQ_LIBRARIES}
)

target_compile_options(system_monitor_lib PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-g -O0 -DDEBUG>
)
