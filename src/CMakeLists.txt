cmake_minimum_required(VERSION 3.16)
project(dynamic_power_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

find_package(Qt6 REQUIRED COMPONENTS Core DBus)
find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core DBus Widgets)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

set(CMAKE_AUTOMOC ON)

add_executable(dynamic_power
    main.cpp
    daemon/daemon.cpp
    daemon/daemon.h
    daemon/cpu_targets.cpp
    daemon/cpu_targets.h
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
    userctl/UserConfig.cpp
    userctl/UserConfig.h
    userctl/TrayController.cpp
    userctl/TrayController.h
    userctl/MainWindow.cpp
    userctl/MainWindow.h
    userctl/RootFeaturesDialog.cpp
    userctl/RootFeaturesDialog.h
    userctl/LoadGraphWidget.cpp
    userctl/LoadGraphWidget.h
    userctl/ProcessMonitor.cpp
    userctl/ProcessMonitor.h
    userctl/ProcessRuleEditor.cpp
    userctl/ProcessRuleEditor.h
    userctl/UPowerClient.cpp
    userctl/UPowerClient.h
    userctl/ProfileConfigDialog.cpp
    userctl/ProfileConfigDialog.h
    userctl/UserFeatures.cpp
    userctl/UserFeatures.h
)

# Feature module sources (scaffold + first extractions)
target_sources(dynamic_power_user PRIVATE
    userctl/features/FeatureBase.cpp
    userctl/features/FeatureBase.h
    userctl/features/FeatureRegistry.cpp
    userctl/features/FeatureRegistry.h
    userctl/features/RootFeatureDetector.cpp
    userctl/features/RootFeatureDetector.h
    userctl/features/ScreenRefreshFeature.cpp
    userctl/features/ScreenRefreshFeature.h
    userctl/features/RootCompositeFeature.cpp
    userctl/features/RootCompositeFeature.h
    userctl/features/PanelAutohideFeature.cpp
    userctl/features/PanelAutohideFeature.h    
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

# ---- Install rules --------------------------------------------------------
include(GNUInstallDirs)

# Install destinations.
set(SYSTEMD_SYSTEM_DIR        "${CMAKE_INSTALL_PREFIX}/lib/systemd/system" CACHE PATH "systemd system unit dir")
set(DBUS_SYSTEM_POLICY_DIR    "/etc/dbus-1/system.d"                       CACHE PATH "D-Bus system policy dir")
set(DESKTOP_DIR               "${CMAKE_INSTALL_DATADIR}/applications"      CACHE PATH "XDG desktop files dir")
set(PIXMAPS_DIR               "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps" CACHE PATH "Icons dir")
set(SHARE_DIR                 "/usr/share/dynamic-power"                   CACHE PATH "Project shared data dir")

# Binaries
install(TARGETS dynamic_power RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS dynamic_power_user RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# systemd unit
install(FILES
    resources/systemd/dynamic_power.service
    DESTINATION "${SYSTEMD_SYSTEM_DIR}"
)

# D-Bus policy
install(FILES
    resources/dbus/org.dynamic_power.Daemon.conf
    DESTINATION "${DBUS_SYSTEM_POLICY_DIR}"
)

# Desktop entry
install(FILES
    resources/dynamic-power.desktop
    DESTINATION "${DESKTOP_DIR}"
)

# Icons
install(FILES
    resources/dynamic_power.default_ac.svg
    resources/dynamic_power.default_battery.svg
    resources/dynamic_power.match_ac.svg
    resources/dynamic_power.match_battery.svg
    resources/dynamic_power.override_ac.svg
    resources/dynamic_power.override_battery.svg
    DESTINATION "${PIXMAPS_DIR}"
)

# YAML templates
install(FILES
    resources/dynamic-power-user.yaml
    resources/dynamic_power.yaml
    DESTINATION "${SHARE_DIR}"
)

# Tests. BUILD_TESTING is on by default when this is included; Qt6::Test comes from
# qt6-base, which is already a dependency, so nothing new is required to build them.
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
