cmake_minimum_required(VERSION 3.16)
project(Sponza)

set(CMAKE_CXX_STANDARD 20)
set(BIN_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BIN_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BIN_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIRECTORY})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-DPROFILE)
endif ()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE INCLUDE CONFIGURE_DEPENDS src/*.hpp)

add_library(Sponza SHARED "${SOURCES}" Sponza.def "${INCLUDE}")

set_target_properties(Sponza
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${BIN_DIRECTORY}"
        LIBRARY_OUTPUT_DIRECTORY "${BIN_DIRECTORY}"
        RUNTIME_OUTPUT_DIRECTORY "${BIN_DIRECTORY}"
)

# add_custom_command(TARGET Sponza POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Sponza> ${BIN_DIRECTORY}
# )

target_include_directories(Sponza PUBLIC ${PROJECT_SOURCE_DIR}/src)

find_package(PandaSDK CONFIG REQUIRED)
target_link_libraries(Sponza PandaSDK::PandaSDK)
