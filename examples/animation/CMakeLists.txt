cmake_minimum_required(VERSION 3.20)
project(AnimationTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++20")

find_package(VKH REQUIRED)

find_library(FBX_LIB fbxsdk HINTS "build" REQUIRED)

add_executable(${PROJECT_NAME} src/main.cpp)

target_link_libraries(${PROJECT_NAME} VKH::VKH ${FBX_LIB})
