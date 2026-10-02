cmake_minimum_required(VERSION 3.21) # Higher version for better Qt6 & FetchContent support 
project(UnifiedConkyControlCenter VERSION 1.1.14 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20) # Upgraded to 20 for better std::filesystem and modern Qt 6.8 features
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Automated Versioning Logic ---
# Release CI passes -DAPP_VERSION_OVERRIDE=<x.y.z> so package names never depend
# on the depth or state of the git checkout that happens to be building.
set(APP_VERSION_OVERRIDE "" CACHE STRING "Explicit x.y.z version; bypasses git describe")

if(APP_VERSION_OVERRIDE)
    set(APP_VERSION "${APP_VERSION_OVERRIDE}")
else()
    # Attempt to get the version from Git tags
    execute_process(
        COMMAND git describe --tags --always
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Clean up the tag (remove leading 'v' if present)
    string(REGEX REPLACE "^v" "" APP_VERSION "${GIT_VERSION}")

    # `--always` falls back to a bare commit SHA when no tag is reachable
    # (shallow clone, detached source tarball). Treat that as "no version".
    if(NOT APP_VERSION MATCHES "^[0-9]+\\.[0-9]+")
        set(APP_VERSION "${PROJECT_VERSION}")
    endif()
endif()

# Two views of the same number:
#   APP_VERSION          what the About box and crash reports show. Keeps the
#                        "-<N>-g<sha>" git describe appends once HEAD is past
#                        the last tag, so an untagged build identifies itself.
#   APP_PACKAGE_VERSION  what CPack names the archives. Always a bare x.y.z:
#                        install.sh rebuilds package file names from its own
#                        VERSION_NUM, so a suffix here would break installing
#                        from any master build and turn CI's package-check red
#                        after every commit that isn't exactly on a tag.
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" APP_PACKAGE_VERSION "${APP_VERSION}")
if(NOT APP_PACKAGE_VERSION)
    set(APP_PACKAGE_VERSION "${PROJECT_VERSION}")
endif()

message(STATUS "Building version: ${APP_VERSION} (packages: ${APP_PACKAGE_VERSION})")

# Enable ccache if available to speed up rebuilds across different build directories
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# Standard Qt 6 Tooling 
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF) # Disabled UIC because app uses programmatic UI and not Qt .ui forms

# 1. Flexible Qt 6 Find Package
# Lowered to 6.4 to support major LTS distributions like Ubuntu 24.04 and Debian 12.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Network Gui)

# Use standard GNU installation directories (bin, share, etc.)
include(GNUInstallDirs)

# 2. Simplified nlohmann_json Handling 
find_package(nlohmann_json 3.11.0 QUIET)
if(NOT nlohmann_json_FOUND)
    # Prevent the library from building its own tests and documentation
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    set(JSON_Install OFF CACHE INTERNAL "")
    
    find_package(Git REQUIRED)
    include(FetchContent)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(json)
    # Create the namespaced alias for the fetched target to match target_link_libraries
    if(NOT TARGET nlohmann_json::nlohmann_json)
        add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
    endif()
endif()

# 3. Source and Header Organization [cite: 3, 4]
set(SOURCES
    src/main.cpp
    src/config_manager.cpp
    src/config_parser.cpp
    src/conky_manager.cpp
    src/theme_manager.cpp
    src/ui_manager.cpp
    src/utils.cpp
    src/screens.cpp
    src/placement.cpp
    src/placement_io.cpp
    src/monitor_map_widget.cpp
    src/first_run_setup.cpp
    src/conky_syntax_highlighter.cpp
    src/in_app_editor.cpp
    src/logger.cpp
    src/error_handler.cpp
    src/error_dialog.cpp
    src/crash_recovery.cpp
    src/error_report_dialog.cpp
    src/display_server.cpp
    src/progress_dialog.cpp
    src/preferences_dialog.cpp
    src/system_detector.cpp
    src/setup_wizard.cpp
    src/hardware_detector.cpp
    src/hardware_detector.h

    # Q_OBJECT headers have to be listed here explicitly: AUTOMOC only mocs
    # headers it is told about, and a header in include/ with no same-named
    # entry silently produces undefined vtable/signal references at link time.
    include/first_run_setup.h
    include/conky_syntax_highlighter.h
    include/in_app_editor.h
    include/error_dialog.h
    include/error_report_dialog.h
    include/progress_dialog.h
    include/preferences_dialog.h
    include/system_detector.h
    include/setup_wizard.h
    include/monitor_map_widget.h
)

# 4. Create Executable 
add_executable(${PROJECT_NAME} ${SOURCES} ${UI_FILES} resources.qrc)

# 5. Modern Include & Linkage (No more Qt5/Qt6 if-else blocks) 
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Gui
    nlohmann_json::nlohmann_json
)

# Pass project version to the source code so it matches the GitHub release automatically
target_compile_definitions(${PROJECT_NAME} PRIVATE APP_VERSION_STR="${APP_VERSION}")

# 6. Installation Logic
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES config/app_config.json DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/config)
install(FILES resources/unified-conky-control-center.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/icon-256.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        RENAME UnifiedConkyControlCenter.png)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})

# 7. CPack Configuration [cite: 6] - Supports DEB, RPM, and TGZ
set(CPACK_PACKAGE_NAME "UnifiedConkyControlCenter")
set(CPACK_DEBIAN_PACKAGE_NAME "unified-conky-control-center")
set(CPACK_RPM_PACKAGE_NAME "unified-conky-control-center")
set(CPACK_PACKAGE_VERSION "${APP_PACKAGE_VERSION}")
set(CPACK_PACKAGE_VENDOR "Unified Conky Control Center")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A unified control center for managing Conky configurations across X11 and Wayland")
set(CPACK_PACKAGE_DESCRIPTION "Unified Conky Control Center provides a graphical interface to manage, edit, and control Conky panel configurations on both X11 and Wayland display servers.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/vamps-goes-coding/UnifiedConkyControlCenter")
set(CPACK_PACKAGE_CONTACT "Unified Conky Control Center Team")

# Debian/Ubuntu Package Configuration
# Let dpkg-shlibdeps derive the Qt runtime deps from the linked binaries when it
# is available — hardcoded Qt package names change across releases (e.g. the
# 24.04 t64 transition), so a static list is always wrong for somebody.
# `conky` stays manual because it is not a linked library.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "conky")
find_program(DPKG_SHLIBDEPS_EXECUTABLE dpkg-shlibdeps)
if(DPKG_SHLIBDEPS_EXECUTABLE)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_LIBRARY_DIRS "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
else()
    message(STATUS "dpkg-shlibdeps not found - DEB will only declare a 'conky' dependency")
endif()
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/vamps-goes-coding/UnifiedConkyControlCenter")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SECTION "x11")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")

# RPM Package Configuration
# rpmbuild auto-generates Requires for the linked ELF sonames, so only the
# non-linked runtime dependency needs to be declared here.
set(CPACK_RPM_PACKAGE_REQUIRES "conky")
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_URL "https://github.com/vamps-goes-coding/UnifiedConkyControlCenter")
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")

# TGZ/Archive Configuration
set(CPACK_SYSTEM_NAME "Linux-x86_64")
set(CPACK_ARCHIVE_FILE_NAME "UnifiedConkyControlCenter-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")

# Common CPack Settings
set(CPACK_GENERATOR "DEB;RPM;TGZ")
set(CPACK_COMPONENTS_GROUPING IGNORE)

include(CPack)

# 8. Tests
# Deliberately Qt-free: the core (parsing, config persistence, panel
# discovery, display-server resolution) is pure C++/POSIX, so the unit tests
# build in seconds and can run in CI without a display server.
option(UCCC_BUILD_TESTS "Build the unit tests" ON)

if(UCCC_BUILD_TESTS)
    enable_testing()

    add_executable(uccc_unit_tests
        tests/unit_tests.cpp
        src/config_parser.cpp
        src/config_manager.cpp
        src/display_server.cpp
        src/placement.cpp
        src/utils.cpp
        src/logger.cpp
    )
    set_target_properties(uccc_unit_tests PROPERTIES AUTOMOC OFF AUTORCC OFF)
    target_include_directories(uccc_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_definitions(uccc_unit_tests PRIVATE APP_VERSION_STR="${APP_VERSION}")
    target_link_libraries(uccc_unit_tests PRIVATE nlohmann_json::nlohmann_json)

    add_test(NAME unit_tests COMMAND uccc_unit_tests)
endif()