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

#[[ GLM ]]
if (DEFINED ENV{PATH_TO_GLM_INCLUDE}) 
	set(GLM_INCLUDE $ENV{PATH_TO_GLM_INCLUDE})
else () 
	find_path(GLM_INCLUDE NAMES glm.hpp PATH_SUFFIXES include/glm/)
endif ()
include_directories(${GLM_INCLUDE})

#[[ FreeType ]]
if (DEFINED ENV{PATH_TO_FREETYPE_INCLUDE}) 
	set(FREETYPE_INCLUDE $ENV{PATH_TO_FREETYPE_INCLUDE})
else () 
	find_path(FREETYPE_INCLUDE NAMES UI.h ONLY_CMAKE_FIND_ROOT_PATH)
endif ()

if (DEFINED ENV{PATH_TO_FREETYPE_LIB}) 
	set(FREETYPE_LIB $ENV{PATH_TO_FREETYPE_LIB})
else () 
	find_library(FREETYPE_LIB NAMES UI)
endif ()
include_directories(${FREETYPE_INCLUDE})
link_directories(${FREETYPE_LIB})

#[[ UsMInt ]]
if (DEFINED ENV{PATH_TO_USMINT_INCLUDE}) 
	set(USMINT_INCLUDE $ENV{PATH_TO_USMINT_INCLUDE})
else () 
	find_path(USMINT_INCLUDE NAMES UI.h ONLY_CMAKE_FIND_ROOT_PATH)
endif ()

if (DEFINED ENV{PATH_TO_USMINT_LIB}) 
	set(USMINT_LIB $ENV{PATH_TO_USMINT_LIB})
else () 
	find_library(USMINT_LIB NAMES UI)
endif ()
include_directories(${USMINT_INCLUDE})
link_directories(${USMINT_LIB})

add_executable(VulkanProject 
	src/main.cpp 
	src/GraphicsHandler.cpp src/GraphicsHandler.h
	src/Errors.cpp src/Errors.h
	src/UIHandler.cpp src/UIHandler.h)

target_link_libraries(${PROJECT_NAME} vulkan.1.2.176 SDL3 UI freetype bz2 -m64)
