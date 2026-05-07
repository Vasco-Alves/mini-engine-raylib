cmake_minimum_required(VERSION 3.20)

project(lua C)

# Grab all the .c files in the src directory
file(GLOB LUA_SOURCES "src/*.c")

# Remove the standalone interpreters
list(REMOVE_ITEM LUA_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lua.c" 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/luac.c"
)

# Create the static library
add_library(lua STATIC ${LUA_SOURCES})

# Expose the include directory so the engine can find lua.h
target_include_directories(lua PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")