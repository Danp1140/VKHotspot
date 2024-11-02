cmake_minimum_required(VERSION 3.20)
project(VulkanProject)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 -Wno-deprecated")
if (UNIX)
	if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Metal -framework Foundation -framework QuartzCore -framework IOKit -framework IOSurface -framework Cocoa -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework CoreServices -framework Carbon -framework CoreFoundation -framework Security")
		set(CMAKE_FIND_LIBRARY_SUFFIXES ".dylib" ".a")
	else()
		set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")
	endif()
endif()


find_package(Freetype REQUIRED)
find_package(Vulkan REQUIRED)
# would love to use SDL3's package, but it's not working out with the include directory
# find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared Headers)

find_path(SDL_INCLUDE SDL3/SDL.h REQUIRED)
find_path(GLM_INCLUDE glm.hpp PATH_SUFFIXES glm REQUIRED)
find_path(USMINT_INCLUDE UI.h PATH_SUFFIXES UsMInt REQUIRED)

find_library(SDL_LIB SDL3 REQUIRED)
find_library(USMINT_LIB UsMInt REQUIRED)

include_directories(
	${FREETYPE_INCLUDE_DIRS} 
	${VULKAN_INCLUDE_DIRS} 
	${SDL_INCLUDE}
	${GLM_INCLUDE}
	${USMINT_INCLUDE})

add_executable(VulkanProject 
	src/main.cpp 
	src/GraphicsHandler.cpp src/GraphicsHandler.h
	src/Errors.cpp src/Errors.h
	src/UIHandler.cpp src/UIHandler.h)

target_link_libraries(
	${PROJECT_NAME} 
	${USMINT_LIB} 
	${FREETYPE_LIBRARIES} 
	Vulkan::Vulkan
	${SDL_LIB})
