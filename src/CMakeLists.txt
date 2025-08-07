cmake_minimum_required(VERSION 3.16)
project(dynamic_power_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

add_executable(dynamic_power_cpp
    main.cpp
    daemon/daemon.cpp
    daemon/daemon.h
    daemon/log.cpp
    daemon/log.h
)

target_include_directories(dynamic_power_cpp PRIVATE ${SYSTEMD_INCLUDE_DIRS})
target_link_libraries(dynamic_power_cpp
    Qt6::Core
    Qt6::DBus
    ${SYSTEMD_LIBRARIES}
)
