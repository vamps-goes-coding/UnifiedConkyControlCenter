cmake_minimum_required(VERSION 3.16)
project(UnifiedConkyControlCenter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Find Qt packages
find_package(Qt6 COMPONENTS Core Widgets Network QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets Network)
endif()

# Find nlohmann_json
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    # Try to find it via pkg-config
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(NLOHMANN_JSON nlohmann_json)
    endif()
    
    if(NOT NLOHMANN_JSON_FOUND)
        # Download nlohmann_json if not found
        include(FetchContent)
        FetchContent_Declare(
            json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
        )
        FetchContent_MakeAvailable(json)
        set(NLOHMANN_JSON_LIBRARIES nlohmann_json::nlohmann_json)
    else()
        set(NLOHMANN_JSON_LIBRARIES ${NLOHMANN_JSON_LIBRARIES})
    endif()
else()
    set(NLOHMANN_JSON_LIBRARIES nlohmann_json::nlohmann_json)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/config_manager.cpp
    src/config_parser.cpp
    src/conky_manager.cpp
    src/theme_manager.cpp
    src/ui_manager.cpp
    src/utils.cpp
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
)

set(HEADERS
    include/app_info.h
    include/config_manager.h
    include/config_parser.h
    include/conky_manager.h
    include/theme_manager.h
    include/ui_manager.h
    include/utils.h
    include/first_run_setup.h
    include/conky_syntax_highlighter.h
    include/in_app_editor.h
    include/logger.h
    include/error_handler.h
    include/error_dialog.h
    include/crash_recovery.h
    include/error_report_dialog.h
    include/display_server.h
    include/progress_dialog.h
    include/preferences_dialog.h
)

# Resources
set(RESOURCES
    resources.qrc
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link libraries
if(Qt6_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        ${NLOHMANN_JSON_LIBRARIES}
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt5::Core
        Qt5::Widgets
        Qt5::Network
        ${NLOHMANN_JSON_LIBRARIES}
    )
endif()

# Link filesystem library for older compilers
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
endif()

# Install configuration file
install(FILES config/app_config.json
    DESTINATION share/${PROJECT_NAME}/config
)

# Install desktop file
install(FILES resources/conky-control-center.desktop
    DESTINATION share/applications
)

# Install icon
install(FILES resources/icon.png
    DESTINATION share/icons/hicolor/256x256/apps
    RENAME ${PROJECT_NAME}.png
)

# Install executable
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# CPack configuration
set(CPACK_PACKAGE_NAME "Unified-Conky-Control-Center")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION "A graphical interface for managing Conky panels, themes, and configurations")
set(CPACK_PACKAGE_CONTACT "Conky Community")
include(CPack)