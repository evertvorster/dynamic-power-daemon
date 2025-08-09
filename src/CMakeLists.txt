cmake_minimum_required(VERSION 3.16)
project(dynamic_power_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus)
find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core DBus Widgets)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

set(CMAKE_AUTOMOC ON)

add_executable(dynamic_power
    main.cpp
    daemon/daemon.cpp
    daemon/daemon.h
    daemon/log.cpp
    daemon/log.h
    daemon/daemon_dbus_interface.cpp
    daemon/daemon_dbus_interface.h
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
# --- User-space utility target ---
add_executable(dynamic_power_user
    userctl/main.cpp
    userctl/App.cpp
    userctl/App.h
    userctl/DbusClient.cpp
    userctl/DbusClient.h
    userctl/Config.cpp
    userctl/Config.h
    userctl/TrayController.cpp
    userctl/TrayController.h
    userctl/MainWindow.cpp
    userctl/MainWindow.h
    userctl/LoadGraphWidget.cpp
    userctl/LoadGraphWidget.h
    userctl/ProcessMonitor.cpp
    userctl/ProcessMonitor.h
    userctl/ProcessRuleEditor.cpp
    userctl/ProcessRuleEditor.h
)

target_include_directories(dynamic_power_user
    PRIVATE
    ${SYSTEMD_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(dynamic_power_user
    Qt6::Core
    Qt6::DBus
    Qt6::Widgets
    yaml-cpp
)
