cmake_minimum_required(VERSION 3.5)

project(player)

if (MSVC)
    add_compile_options(/utf-8)  # 让编译器使用UTF-8编码
    add_link_options(-NODEFAULTLIB:libucrt.lib ucrt.lib)  # 不使用默认库，防止冲突
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
include(CheckIncludeFiles)
find_package(AVFORMAT 58 REQUIRED)
find_package(AVCODEC 58 REQUIRED)
find_package(AVDEVICE 58 REQUIRED)
find_package(AVUTIL 56 REQUIRED)
find_package(AVFILTER 7 REQUIRED)
find_package(SWRESAMPLE 4 REQUIRED)
find_package(SDL2 REQUIRED)

set(ENABLE_ICONV OFF CACHE BOOL "Libiconv is not needed.")
add_subdirectory(utils)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/utils")

add_executable(build_tools build_tools.cpp)
target_link_libraries(build_tools utils)

set(CORE_FILES
player.h
src/core.h
src/core.cpp
src/open.h
src/open.c
src/decode.h
src/decode.c
src/audio_output.h
src/audio_output.c
src/loop.h
src/loop.c
src/video_output.h
src/video_output.c
"${CMAKE_CURRENT_BINARY_DIR}/player.rc"
"${CMAKE_CURRENT_BINARY_DIR}/player_version.h"
)

set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/player.rc" PROPERTIES GENERATED TRUE)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/player_version.h" PROPERTIES GENERATED TRUE)

add_custom_target(player_version
    ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/PLAYER_VERSION.cmake"
    COMMAND "$<TARGET_FILE:build_tools>" "${CMAKE_CURRENT_BINARY_DIR}/player.rc"
)
add_dependencies(player_version build_tools)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(PUBLIC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/player.h" "${CMAKE_CURRENT_BINARY_DIR}/player_version.h")

add_library(player SHARED "${CORE_FILES}")
add_dependencies(player player_version)
set_target_properties(player PROPERTIES PREFIX "")
set_target_properties(player PROPERTIES PUBLIC_HEADER "${PUBLIC_HEADERS}")
target_compile_definitions(player PRIVATE -DBUILD_PLAYER)
target_link_libraries(player AVFORMAT::AVFORMAT)
target_link_libraries(player AVCODEC::AVCODEC)
target_link_libraries(player AVDEVICE::AVDEVICE)
target_link_libraries(player AVUTIL::AVUTIL)
target_link_libraries(player AVFILTER::AVFILTER)
target_link_libraries(player SWRESAMPLE::SWRESAMPLE)
target_link_libraries(player SDL2::Core)
if (SDL2MAIN_FOUND)
    target_link_libraries(player SDL2::Main)
endif()
target_link_libraries(player utils)

if (NOT MSVC)
    target_link_libraries(player m)
endif()

if (NOT WIN32)
    target_link_libraries(player Threads::Threads)
endif()

add_executable(test_play WIN32 test/test_play.cpp)
target_link_libraries(test_play player)

install(TARGETS player)
if (MSVC)
    install(FILES $<TARGET_PDB_FILE:player> DESTINATION bin OPTIONAL)
endif()
