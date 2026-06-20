cmake_minimum_required(VERSION 3.15)

project(
    peer_pressure
    VERSION 1.0
    LANGUAGES CXX
)

# --------------------------------------------------
# C++ Standard
# --------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --------------------------------------------------
# Default Build Type
# --------------------------------------------------

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# --------------------------------------------------
# Executable
# --------------------------------------------------

add_executable(
    torrent

    main.cpp

    bencode.cpp
    tracker.cpp
    peer.cpp

    peer_manager.cpp
    piece_manager.cpp
    scheduler.cpp

    download_worker.cpp
    torrent_session.cpp
    file_manager.cpp

    utils.cpp
    sha1.cpp

    tui.cpp
    ftxui_renderer.cpp
)

# --------------------------------------------------
# Include Directories
# --------------------------------------------------

target_include_directories(
    torrent
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# --------------------------------------------------
# FTXUI Setup via FetchContent
# --------------------------------------------------

include(FetchContent)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
    GIT_TAG v7.0.0
)

# Disable docs, examples, and tests for fast compilation
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(ftxui)

# --------------------------------------------------
# Dependencies
# --------------------------------------------------

find_package(CURL REQUIRED)

target_link_libraries(
    torrent
    PRIVATE
    CURL::libcurl
    ftxui::screen
    ftxui::dom
)

# --------------------------------------------------
# Windows Networking
# --------------------------------------------------

if(WIN32)
    target_link_libraries(
        torrent
        PRIVATE
        ws2_32
    )
endif()

# --------------------------------------------------
# Compiler Warnings
# --------------------------------------------------

if(MSVC)
    target_compile_options(
        torrent
        PRIVATE
        /W4
    )
else()
    target_compile_options(
        torrent
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()