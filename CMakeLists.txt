cmake_minimum_required(VERSION 3.28)
project(hcpp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(spdlog REQUIRED)

add_executable(hcpp main.cpp)

target_link_libraries(hcpp PRIVATE spdlog::spdlog)

add_executable(a2 a.cpp)



