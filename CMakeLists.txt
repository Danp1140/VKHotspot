cmake_minimum_required(VERSION 3.20)
project(VulkanProject)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 -Wno-deprecated")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Metal -framework Foundation -framework QuartzCore -framework IOKit -framework IOSurface -framework Cocoa -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework CoreServices -framework Carbon -framework CoreFoundation -framework Security")

set(CMAKE_FIND_ROOT_PATH /usr/local/)

#[[ Vulkan ]]
if (DEFINED ENV{PATH_TO_VULKAN_INCLUDE}) 
	set(VULKAN_INCLUDE $ENV{PATH_TO_VULKAN_INCLUDE})
else () 
	find_path(VULKAN_INCLUDE NAMES vulkan.h)
endif ()

if (DEFINED ENV{PATH_TO_VULKAN_LIB}) 
	set(VULKAN_LIB $ENV{PATH_TO_VULKAN_LIB})
else () 
	find_library(VULKAN_LIB NAMES vulkan libvulkan)
endif ()
include_directories(${VULKAN_INCLUDE})
link_directories(${VULKAN_LIB})

message(WARNING ${VULKAN_INCLUDE})
message(WARNING ${VULKAN_LIB})

#[[ SDL ]]
if (DEFINED ENV{PATH_TO_SDL_INCLUDE}) 
	set(SDL_INCLUDE $ENV{PATH_TO_SDL_INCLUDE})
else () 
	find_path(SDL_INCLUDE NAMES sdl.h ONLY_CMAKE_FIND_ROOT_PATH)
endif ()

if (DEFINED ENV{PATH_TO_SDL_LIB}) 
	set(SDL_LIB $ENV{PATH_TO_SDL_LIB})
else () 
	find_library(SDL_LIB NAMES sdl libsdl)
endif ()
include_directories(${SDL_INCLUDE})
link_directories(${SDL_LIB})

message(WARNING ${SDL_INCLUDE})
message(WARNING ${SDL_LIB})

#[[ GLM ]]
if (DEFINED ENV{PATH_TO_GLM_INCLUDE}) 
	set(GLM_INCLUDE $ENV{PATH_TO_GLM_INCLUDE})
else () 
	find_path(GLM_INCLUDE NAMES glm.hpp PATH_SUFFIXES include/glm/)
endif ()
include_directories(${GLM_INCLUDE})

message(WARNING ${GLM_INCLUDE})


add_executable(VulkanProject 
	src/main.cpp 
	src/GraphicsHandler.cpp src/GraphicsHandler.h
	src/Errors.cpp src/Errors.h)

target_link_libraries(${PROJECT_NAME} vulkan.1.2.176 SDL2 -m64)
