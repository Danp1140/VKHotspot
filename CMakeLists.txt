cmake_minimum_required(VERSION 3.20)
project(VulkanProject)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 -Wno-deprecated")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Metal -framework Foundation -framework QuartzCore -framework IOKit -framework IOSurface -framework Cocoa -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework CoreServices -framework Carbon -framework CoreFoundation -framework Security")
set(CMAKE_IGNORE_PATH "/Library/Frameworks/")

find_path(VULKAN_INCLUDE_PATH vulkan.h PATH_SUFFIXES vulkan  REQUIRED)
include_directories(${VULKAN_INCLUDE_PATH})
find_library(VULKAN_LIB_PATH vulkan.1.3.216 REQUIRED)
link_directories(${VULKAN_LIB_PATH})

find_path(SDL_INCLUDE SDL3/SDL.h REQUIRED)
include_directories(${SDL_INCLUDE})
find_library(SDL_LIB SDL3 REQUIRED)
link_directories(${SDL_LIB})

find_path(GLM_INCLUDE glm.hpp PATH_SUFFIXES glm REQUIRED)
include_directories(${GLM_INCLUDE})

find_path(FREETYPE_INCLUDE ft2build.h PATH_SUFFIXES freetype2 REQUIRED) 
include_directories(${FREETYPE_INCLUDE})
find_library(FREETYPE_LIB freetype REQUIRED)
link_directories(${FREETYPE_LIB})

set(USMINT_INCLUDE $ENV{PATH_TO_USMINT_INCLUDE})
set(USMINT_LIB $ENV{PATH_TO_USMINT_LIB})
include_directories(${USMINT_INCLUDE})
link_directories(${USMINT_LIB})

add_executable(VulkanProject 
	src/main.cpp 
	src/GraphicsHandler.cpp src/GraphicsHandler.h
	src/Errors.cpp src/Errors.h
	src/UIHandler.cpp src/UIHandler.h
	src/Scene.cpp src/Scene.h
	src/Projection.cpp src/Projection.h
	src/Mesh.cpp src/Mesh.h)

target_link_libraries(${PROJECT_NAME} ${VULKAN_LIB_PATH} ${SDL_LIB} UsMInt ${FREETYPE_LIB} bz2 -m64)
