cmake_minimum_required(VERSION 3.16)
project(dynamic_power_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

set(CMAKE_AUTOMOC ON)

add_executable(dynamic_power
    main.cpp
    daemon/daemon.cpp
    daemon/daemon.h
    daemon/log.cpp
    daemon/log.h
    daemon/dbus_adaptor.cpp
    daemon/dbus_adaptor.h
    config/config.cpp
    config/config.h
)

target_include_directories(dynamic_power
    PRIVATE
    ${SYSTEMD_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(dynamic_power
    Qt6::Core
    Qt6::DBus
    yaml-cpp
    ${SYSTEMD_LIBRARIES}
)
