cmake_minimum_required(VERSION 3.20)
project(VKH)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++20 -Wno-deprecated")

if (UNIX)
	if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		set(CMAKE_FIND_FRAMEWORK LAST)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Metal -framework Foundation -framework QuartzCore -framework IOKit -framework IOSurface -framework Cocoa -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework CoreServices -framework Carbon -framework CoreFoundation -framework Security")
		set(CMAKE_PREFIX_PATH "/usr/local/opt/zlib" ${CMAKE_PREFIX_PATH})
		set(ENV{PKG_CONFIG_PATH} "/usr/local/opt/zlib/lib/pkgconfig/")
    set(CMAKE_IGNORE_PATH "/Library/Frameworks/")
		set(CMAKE_FIND_LIBRARY_SUFFIXES ".dylib" ".a")
	else()
		set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")
	endif()
endif()

find_package(Freetype REQUIRED)
find_package(Vulkan REQUIRED)
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared Headers)
find_package(PNG REQUIRED)

find_path(GLM_INCLUDE glm.hpp PATH_SUFFIXES glm REQUIRED)
find_path(USMINT_INCLUDE UI.h PATH_SUFFIXES UsMInt REQUIRED)

find_library(USMINT_LIB UsMInt REQUIRED)

include_directories(
	${FREETYPE_INCLUDE_DIRS} 
	${PNG_INCLUDE_DIRS}
	Vulkan::Headers
	SDL3::Headers
	${GLM_INCLUDE}
	${USMINT_INCLUDE})

add_library(${PROJECT_NAME} ../src/GraphicsHandler.cpp ../src/GraphicsHandler.h
	../src/Errors.cpp ../src/Errors.h
	../src/UIHandler.cpp ../src/UIHandler.h
	../src/Scene.cpp ../src/Scene.h
	../src/Projection.cpp ../src/Projection.h
	../src/Mesh.cpp ../src/Mesh.h
	../src/TextureHandler.cpp ../src/TextureHandler.h
	../src/PhysicsHandler.cpp ../src/PhysicsHandler.h)

target_link_libraries(
	${PROJECT_NAME} 
	${USMINT_LIB} 
	Freetype::Freetype
	Vulkan::Vulkan
	SDL3::SDL3
	PNG::PNG)

install(TARGETS ${PROJECT_NAME}
	LIBRARY DESTINATION /usr/local/lib)
install(FILES ../src/GraphicsHandler.cpp ../src/GraphicsHandler.h
	../src/Errors.cpp ../src/Errors.h
	../src/UIHandler.cpp ../src/UIHandler.h
	../src/Scene.cpp ../src/Scene.h
	../src/Projection.cpp ../src/Projection.h
	../src/Mesh.cpp ../src/Mesh.h
	../src/TextureHandler.cpp ../src/TextureHandler.h
	../src/PhysicsHandler.cpp ../src/PhysicsHandler.h
	DESTINATION /usr/local/include/VKHotspot)
# TODO: install as package
