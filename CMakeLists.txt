cmake_minimum_required(VERSION 3.21) # Higher version for better Qt6 & FetchContent support 
project(UnifiedConkyControlCenter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20) # Upgraded to 20 for better std::filesystem and modern Qt 6.8 features
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# 6. Installation Logic [cite: 6]
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES config/app_config.json DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/config)
install(FILES resources/conky-control-center.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/icon.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME UnifiedConkyControlCenter.png)

# CPack [cite: 6]
set(CPACK_PACKAGE_NAME "Unified-Conky-Control-Center")
include(CPack)