cmake_minimum_required(VERSION 3.20)
project(AnimationTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++20 -Wno-deprecated")

find_package(VKH REQUIRED)

add_executable(${PROJECT_NAME} src/main.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC "/Applications/Autodesk/FBX SDK/2020.3.9/include/")
find_library(LIB_FBX fbxsdk libfbxsdk REQUIRED)
message(WARNING ${LIB_FBX})
# target_link_directories(${PROJECT_NAME} PUBLIC "/Applications/Autodesk/FBX SDK/2020.3.9/lib/clang/release/")
target_link_libraries(${PROJECT_NAME} VKH::VKH ${LIB_FBX})
