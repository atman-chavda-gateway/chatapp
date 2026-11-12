
cmake_minimum_required(VERSION 3.20)

project(chatapp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Dependency management
include(FetchContent)

# Download standalone Asio
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-36-0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(asio)

# Asio is a header-only library
add_library(asio INTERFACE)

target_include_directories(asio SYSTEM INTERFACE
    ${asio_SOURCE_DIR}/asio/include
)

target_compile_definitions(asio INTERFACE
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
)

find_package(Threads REQUIRED)

target_link_libraries(asio INTERFACE Threads::Threads)

if(WIN32)
    target_compile_definitions(asio INTERFACE
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
    )

    target_link_libraries(asio INTERFACE
        ws2_32
        mswsock
    )
endif()

# Application
add_executable(chatapp src/main.cpp)

target_link_libraries(chatapp PRIVATE asio)
