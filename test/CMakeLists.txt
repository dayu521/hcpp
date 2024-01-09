cmake_minimum_required(VERSION 3.28)
project(hcpp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

# find_package(spdlog REQUIRED)

# set(appname ${PROJECT_NAME})

file(GLOB src  "src/*.cpp" )


add_executable(test1 test1.cpp)

add_executable(a2 a.cpp)
add_executable(b b.cpp)

# target_include_directories(${appname} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# target_link_libraries(${appname} PRIVATE spdlog::spdlog)





