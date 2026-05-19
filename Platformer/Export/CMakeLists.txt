cmake_minimum_required(VERSION 3.20)
project(Platformer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF)

# Platform detection
if(APPLE)
    if(IOS)
        set(PLATFORM_IOS TRUE)
    else()
        set(PLATFORM_MACOS TRUE)
    endif()
elseif(ANDROID)
    set(PLATFORM_ANDROID TRUE)
elseif(WIN32)
    set(PLATFORM_WIN32 TRUE)
else()
    set(PLATFORM_LINUX TRUE)
endif()

# Find PandaSDK - provides pre-compiled runtime + scripting API
find_package(PandaSDK CONFIG REQUIRED)

if(NOT EXISTS "${PandaSDK_ENGINE_COMMON_RESOURCES_DIR}")
    message(FATAL_ERROR "PandaSDK common resources not found: ${PandaSDK_ENGINE_COMMON_RESOURCES_DIR}")
endif()

if(NOT EXISTS "${PandaSDK_ENGINE_SHADER_RESOURCES_DIR}")
    message(FATAL_ERROR "PandaSDK shader resources not found: ${PandaSDK_ENGINE_SHADER_RESOURCES_DIR}")
endif()

set(APP_SOURCES
    App/StartApp.cpp
)

get_filename_component(GAME_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(GAME_SCRIPT_DIR "${GAME_PROJECT_ROOT}/Scripts/Platformer/src")

if(EXISTS "${GAME_SCRIPT_DIR}")
    file(GLOB_RECURSE GAME_SCRIPT_SOURCES CONFIGURE_DEPENDS
        "${GAME_SCRIPT_DIR}/*.cpp"
        "${GAME_SCRIPT_DIR}/*.cc"
        "${GAME_SCRIPT_DIR}/*.cxx"
        "${GAME_SCRIPT_DIR}/*.mm"
    )
    file(GLOB_RECURSE GAME_SCRIPT_HEADERS CONFIGURE_DEPENDS
        "${GAME_SCRIPT_DIR}/*.hpp"
        "${GAME_SCRIPT_DIR}/*.h"
    )
    list(APPEND APP_SOURCES ${GAME_SCRIPT_SOURCES} ${GAME_SCRIPT_HEADERS})
else()
    message(STATUS "No game script source directory found: ${GAME_SCRIPT_DIR}")
endif()

if(PLATFORM_MACOS OR PLATFORM_LINUX OR PLATFORM_WIN32)
    list(APPEND APP_SOURCES EntryPoint/Desktop/main.cpp)
elseif(PLATFORM_IOS)
    list(APPEND APP_SOURCES EntryPoint/IOS/main.mm)
endif()

# ---- App executable ----
# Platform entry point and app startup are generated in this project.
# Engine/runtime implementation is pre-compiled in PandaSDK.
if(PLATFORM_MACOS)
    set(APPLE_RESOURCES native/OSX/AssetsOSX.xcassets)
    add_executable(Platformer MACOSX_BUNDLE ${APP_SOURCES} ${APPLE_RESOURCES})
    set_target_properties(Platformer PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/native/OSX/Info.plist"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.md.Platformer"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        RESOURCE "${APPLE_RESOURCES}"
    )
    target_compile_definitions(Platformer PUBLIC GL_SILENCE_DEPRECATION)
elseif(PLATFORM_IOS)
    set(APPLE_RESOURCES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/native/IOS/LaunchScreen.storyboard")
        list(APPEND APPLE_RESOURCES native/IOS/LaunchScreen.storyboard)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/native/IOS/Assets.xcassets")
        list(APPEND APPLE_RESOURCES native/IOS/Assets.xcassets)
    endif()
    add_executable(Platformer MACOSX_BUNDLE ${APP_SOURCES} ${APPLE_RESOURCES})
    set_target_properties(Platformer PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/native/IOS/Info.plist"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.md.Platformer"
        RESOURCE "${APPLE_RESOURCES}"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer"
        BUILD_RPATH "@executable_path"
    )
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/native/IOS/Assets.xcassets")
        set_target_properties(Platformer PROPERTIES XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
    endif()
    target_compile_definitions(Platformer PUBLIC GL_SILENCE_DEPRECATION GLES_SILENCE_DEPRECATION)
elseif(PLATFORM_LINUX)
    add_executable(Platformer ${APP_SOURCES})
elseif(PLATFORM_WIN32)
    add_executable(Platformer WIN32 ${APP_SOURCES})
elseif(PLATFORM_ANDROID)
    add_library(Platformer SHARED ${APP_SOURCES})
endif()

# Link against pre-compiled runtime. Entry point remains in generated app sources.
target_link_libraries(Platformer PandaSDK::PandaRuntime)
if(EXISTS "${GAME_SCRIPT_DIR}")
    target_include_directories(Platformer PRIVATE "${GAME_SCRIPT_DIR}")
endif()

# Copy engine resources and game Content into fixed runtime locations
if(PLATFORM_MACOS)
    set(APP_RESOURCES_OUTPUT_DIR "$<TARGET_FILE_DIR:Platformer>/../Resources")
elseif(PLATFORM_IOS)
    set(APP_RESOURCES_OUTPUT_DIR "$<TARGET_FILE_DIR:Platformer>")
elseif(PLATFORM_LINUX OR PLATFORM_WIN32)
    set(APP_RESOURCES_OUTPUT_DIR "$<TARGET_FILE_DIR:Platformer>")
endif()

if(PLATFORM_MACOS OR PLATFORM_IOS OR PLATFORM_LINUX OR PLATFORM_WIN32)
    set(CONTENT_OUTPUT_DIR "${APP_RESOURCES_OUTPUT_DIR}/Content")
    add_custom_command(TARGET Platformer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PandaSDK_ENGINE_COMMON_RESOURCES_DIR}"
            "${APP_RESOURCES_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PandaSDK_ENGINE_SHADER_RESOURCES_DIR}"
            "${APP_RESOURCES_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CONTENT_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/Content"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Content"
            "${CONTENT_OUTPUT_DIR}"
        COMMENT "Copying Content to app bundle"
    )
endif()
