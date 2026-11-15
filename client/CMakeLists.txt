
cmake_minimum_required(VERSION 3.25)

project(chatapp-client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Asio
FetchContent_Declare(
    asio
    URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-36-0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_GetProperties(asio)

if(NOT asio_POPULATED)
    FetchContent_MakeAvailable(asio)
endif()

# FTXUI
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v6.1.9
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(ftxui)

# Threads
find_package(Threads REQUIRED)

# Asio interface library
add_library(asio INTERFACE)

target_include_directories(asio SYSTEM INTERFACE
    ${asio_SOURCE_DIR}/asio/include
)

target_compile_definitions(asio INTERFACE
    _WIN32_WINNT=0x0A00
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
)

# Client executable
add_executable(chatapp-client
    src/main.cpp
    src/ui.cpp
    src/network_client.cpp
)

target_link_libraries(chatapp-client PRIVATE
    asio
    Threads::Threads
    ftxui::component
    ftxui::dom
    ftxui::screen
    stdc++exp
    ws2_32
    mswsock
)
