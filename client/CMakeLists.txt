
cmake_minimum_required(VERSION 3.25)

project(chatapp-client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    asio
    URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-36-0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_GetProperties(asio)

if(NOT asio_POPULATED)
    FetchContent_MakeAvailable(asio)
endif()

find_package(Threads REQUIRED)

add_library(asio INTERFACE)

target_include_directories(asio SYSTEM INTERFACE
    ${asio_SOURCE_DIR}/asio/include
)

target_compile_definitions(asio INTERFACE
    _WIN32_WINNT=0x0A00
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
)

add_executable(chatapp-client
    src/main.cpp
)

target_link_libraries(chatapp-client PRIVATE
    asio
    Threads::Threads
    stdc++exp
    ws2_32
    mswsock
)
