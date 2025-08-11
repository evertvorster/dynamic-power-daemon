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
    userctl/UPowerClient.cpp
    userctl/UPowerClient.h
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

# ---- Install rules (match Makefile destinations) --------------------------
include(GNUInstallDirs)

# Mirrors Makefile defaults (Makefile: L15–23)
set(SYSTEMD_SYSTEM_DIR        "${CMAKE_INSTALL_PREFIX}/lib/systemd/system" CACHE PATH "systemd system unit dir")
set(DBUS_SYSTEM_POLICY_DIR    "/etc/dbus-1/system.d"                       CACHE PATH "D-Bus system policy dir")
set(DESKTOP_DIR               "${CMAKE_INSTALL_DATADIR}/applications"      CACHE PATH "XDG desktop files dir")
set(PIXMAPS_DIR               "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps" CACHE PATH "Icons dir")
set(SHARE_DIR                 "/usr/share/dynamic-power"                   CACHE PATH "Project shared data dir")

# Binaries (Makefile: L32–33)
install(TARGETS dynamic_power RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS dynamic_power_user RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# systemd unit (Makefile: L36–37)
install(FILES
    resources/systemd/dynamic_power.service
    DESTINATION "${SYSTEMD_SYSTEM_DIR}"
)

# D-Bus policy (Makefile: L40–41)
install(FILES
    resources/dbus/org.dynamic_power.Daemon.conf
    DESTINATION "${DBUS_SYSTEM_POLICY_DIR}"
)

# Desktop entry (Makefile: L44–45)
install(FILES
    resources/dynamic-power.desktop
    DESTINATION "${DESKTOP_DIR}"
)

# Icons (Makefile: L48–53)
install(FILES
    resources/dynamic_power.default_ac.svg
    resources/dynamic_power.default_battery.svg
    resources/dynamic_power.match_ac.svg
    resources/dynamic_power.match_battery.svg
    resources/dynamic_power.override_ac.svg
    resources/dynamic_power.override_battery.svg
    DESTINATION "${PIXMAPS_DIR}"
)

# YAML templates (Makefile: L56–57)
install(FILES
    resources/dynamic-power-user.yaml
    resources/dynamic_power.yaml
    DESTINATION "${SHARE_DIR}"
)
