#cmake_minimum_required(VERSION 3.28)
#project(hcpp VERSION 0.1.0 LANGUAGES C CXX)
#
#set(CMAKE_CXX_STANDARD 20)
#
#find_package(spdlog REQUIRED)
#
#set(appname ${PROJECT_NAME})
#
#file(GLOB module_src  "src/module/*.ixx" )
#file(GLOB src  "src/*.cpp" )
#
## message(${src})
#
#add_executable(${appname} main.cpp ${src})
#
#target_include_directories(${appname} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
#
#target_link_libraries(${appname} PRIVATE spdlog::spdlog)
#
## target_sources(${appname} PUBLIC  
## FILE_SET CXX_MODULES 
## FILES  ${module_src})
#
#add_subdirectory(test)
