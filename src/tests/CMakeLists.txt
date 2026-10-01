# Tests. QtTest ships in qt6-base, which the project already depends on, so the
# only thing added here is the build wiring.
#
# Each test lists the sources it covers rather than linking a library, so what a
# test depends on is visible in the file instead of implied by the build.

set(DP_SRC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# --- the daemon-side config parser -------------------------------------------
add_executable(tst_config
    tst_config.cpp
    ${DP_SRC}/config/config.cpp
    ${DP_SRC}/daemon/log.cpp
)
target_include_directories(tst_config PRIVATE ${DP_SRC} ${SYSTEMD_INCLUDE_DIRS})
target_link_libraries(tst_config PRIVATE Qt6::Test Qt6::Core yaml-cpp ${SYSTEMD_LIBRARIES})
add_test(NAME tst_config COMMAND tst_config)

# --- which sysfs nodes a configured example path stands for --------------------
add_executable(tst_cpu_targets
    tst_cpu_targets.cpp
    ${DP_SRC}/daemon/cpu_targets.cpp
)
target_include_directories(tst_cpu_targets PRIVATE ${DP_SRC})
target_link_libraries(tst_cpu_targets PRIVATE Qt6::Test Qt6::Core)
add_test(NAME tst_cpu_targets COMMAND tst_cpu_targets)

# --- the read/serialize side of the same file --------------------------------
add_executable(tst_rootcompositefeature
    tst_rootcompositefeature.cpp
    ${DP_SRC}/userctl/features/RootCompositeFeature.cpp
)
target_include_directories(tst_rootcompositefeature PRIVATE ${DP_SRC})
target_link_libraries(tst_rootcompositefeature PRIVATE Qt6::Test Qt6::Core yaml-cpp)
add_test(NAME tst_rootcompositefeature COMMAND tst_rootcompositefeature)

# --- the capability lists, which were previously one list ----------------------
add_executable(tst_profileconfigdialog
    tst_profileconfigdialog.cpp
    ${DP_SRC}/userctl/ProfileConfigDialog.cpp
)
target_include_directories(tst_profileconfigdialog PRIVATE ${DP_SRC})
target_compile_options(tst_profileconfigdialog PRIVATE -fno-access-control)
target_link_libraries(tst_profileconfigdialog PRIVATE Qt6::Test Qt6::Widgets Qt6::Gui Qt6::Core yaml-cpp)
add_test(NAME tst_profileconfigdialog COMMAND tst_profileconfigdialog)

# --- the rule deciding what gets written back ---------------------------------
# -fno-access-control because stateToSave() is private. The alternative is
# including the .cpp with private and public swapped, which breaks the libstdc++
# headers; the flag does not change the class, only what this translation unit
# may name.
add_executable(tst_rootfeaturesdialog
    tst_rootfeaturesdialog.cpp
    ${DP_SRC}/userctl/RootFeaturesDialog.cpp
    ${DP_SRC}/userctl/UserFeatures.cpp
    ${DP_SRC}/userctl/UserConfig.cpp
    ${DP_SRC}/userctl/features/FeatureBase.cpp
    ${DP_SRC}/userctl/features/ScreenRefreshFeature.cpp
    ${DP_SRC}/userctl/features/PanelAutohideFeature.cpp
    ${DP_SRC}/userctl/features/RootCompositeFeature.cpp
    ${DP_SRC}/userctl/features/RootFeatureDetector.cpp
)
target_include_directories(tst_rootfeaturesdialog PRIVATE ${DP_SRC})
target_compile_options(tst_rootfeaturesdialog PRIVATE -fno-access-control)
target_link_libraries(tst_rootfeaturesdialog PRIVATE
    Qt6::Test Qt6::Widgets Qt6::Gui Qt6::Core Qt6::DBus yaml-cpp)
add_test(NAME tst_rootfeaturesdialog COMMAND tst_rootfeaturesdialog)

# Nothing here draws a window, but two of the binaries construct widgets, and an
# offscreen platform keeps that honest without needing a display.
set_tests_properties(tst_rootfeaturesdialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
