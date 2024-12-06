# Project Setup
cmake_minimum_required(VERSION 3.20)
project(VKH)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++20 -Wno-deprecated")

# Platform-Dependent Setup, jank central
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

# Finding Dependencies
find_package(Freetype REQUIRED)
find_package(Vulkan REQUIRED)
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared Headers)
find_package(PNG REQUIRED)
find_package(portaudio REQUIRED)

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

# Adding and Linking Library
add_library(${PROJECT_NAME} ../src/GraphicsHandler.cpp ../src/GraphicsHandler.h
	../src/Errors.cpp ../src/Errors.h
	../src/UIHandler.cpp ../src/UIHandler.h
	../src/Scene.cpp ../src/Scene.h
	../src/Projection.cpp ../src/Projection.h
	../src/Mesh.cpp ../src/Mesh.h
	../src/TextureHandler.cpp ../src/TextureHandler.h
	../src/PhysicsHandler.cpp ../src/PhysicsHandler.h
	../src/InputHandler.cpp ../src/InputHandler.h
	../src/AudioHandler.cpp ../src/AudioHandler.h)

target_include_directories(${PROJECT_NAME} PUBLIC ${GLM_INCLUDE} ${USMINT_INCLUDE})

target_link_libraries(
	${PROJECT_NAME} PUBLIC
	${USMINT_LIB} 
	Freetype::Freetype
	Vulkan::Vulkan
	SDL3::SDL3
	PNG::PNG
	portaudio)

# Installing Library Files
install(
	FILES 
	../src/GraphicsHandler.h
	../src/Errors.h
	../src/UIHandler.h
	../src/Scene.h
	../src/Projection.h
	../src/Mesh.h
	../src/TextureHandler.h
	../src/PhysicsHandler.h
	../src/InputHandler.h
	../src/AudioHandler.h
	DESTINATION /usr/local/include/VKHotspot)

install(
	FILES
	../src/GraphicsHandler.h
	../src/Errors.h
	../src/UIHandler.h
	../src/Scene.h
	../src/Projection.h
	../src/Mesh.h
	../src/TextureHandler.h
	../src/PhysicsHandler.h
	../src/InputHandler.h
	../src/AudioHandler.h
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Package Config Hell
include(GNUInstallDirs)
install(
	TARGETS ${PROJECT_NAME}
	EXPORT ${PROJECT_NAME}Targets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
	EXPORT ${PROJECT_NAME}Targets
	FILE ${PROJECT_NAME}Targets.cmake
	NAMESPACE VKH::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
include(CMakePackageConfigHelpers)
configure_package_config_file(
	${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
	VERSION 1.0.0
	COMPATIBILITY AnyNewerVersion)
install(
	FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

