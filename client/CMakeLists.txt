cmake_minimum_required(VERSION 3.25)

project(chatapp-client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(chatapp-client
    src/main.cpp
)

target_link_libraries(chatapp-client PRIVATE stdc++exp)