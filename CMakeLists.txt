cmake_minimum_required(VERSION 3.20)
project(VulkanProject)

set(PATH_TO_SDL_INCLUDE "/usr/local/cellar/sdl2/2.30.6/include/SDL2")
set(PATH_TO_SDL_LIB "/usr/local/cellar/sdl2/2.30.6/lib")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 -Wno-deprecated")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Metal -framework Foundation -framework QuartzCore -framework IOKit -framework IOSurface -framework Cocoa -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework CoreServices -framework Carbon -framework CoreFoundation -framework Security")

include_directories(include)
include_directories(include/vulkan)
include_directories(${PATH_TO_SDL_INCLUDE})
link_directories(libs)
link_directories(libs/vulkan)
link_directories(${PATH_TO_SDL_LIB})

add_executable(VulkanProject 
	src/main.cpp 
	src/GraphicsHandler.cpp src/GraphicsHandler.h
	src/Errors.cpp src/Errors.h
	)

target_link_libraries(${PROJECT_NAME} vulkan.1.2.176 SDL2 -m64)
