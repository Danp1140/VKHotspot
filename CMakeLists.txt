cmake_minimum_required(VERSION 3.20)
project(VulkanProject)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 -Wno-deprecated")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Metal -framework Foundation -framework QuartzCore -framework IOKit -framework IOSurface -framework Cocoa -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework CoreServices -framework Carbon -framework CoreFoundation -framework Security")

include_directories(include)
include_directories(include/vulkan)
link_directories(libs)
link_directories(libs/vulkan)

add_executable(VulkanProject src/main.cpp src/GraphicsHandler.cpp src/GraphicsHandler.h 
)

target_link_libraries(${PROJECT_NAME} vulkan.1.2.176 glfw3 -m64)
