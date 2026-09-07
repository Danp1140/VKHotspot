# Building

Use cmake.
First run `cmake .`
Then run `cmake --build .`

If you need or wish to specify paths for Vulkan, SDL, and/or GLM, you may set environment variables to this end (but if they are already in your path, it is not required). Such environment variables are formated as `PATH_TO_[LIBRARY]_INCLUDE` or `PATH_TO_[LIBRARY]_LIB`. GLM of course does not have any lib files and so cannot make use of the LIB environment variable. INCLUDE variables should point to the folder containing the top level header file (i.e., if, like Vulkan, the include folder has some `.h`s but also more folders containing more `.h`s, just set `PATH_TO_VULKAN_INCLUDE` to the folder containing the first `.h`). 

So, for example, if you need to explicitly set the location of the SDL library, it might look like:
`PATH_TO_SDL_INCLUDE=/usr/local/Cellar/sdl2/2.30.6/include/SDL2 PATH_TO_SDL_LIB=/usr/local/Cellar/sdl2/2.30.6/lib cmake .`

The current CMakeLists.txt builds to an executable using main.cpp which executes GH's frame callback until the program is signalled to close. At a later date, `CMakeLists.txt` may be modified to compile to a `.a` library for use in custom applications. 
