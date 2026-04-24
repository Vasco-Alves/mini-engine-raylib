# 1. Create the Executable
add_executable(sandbox 
    "src/main.cpp"
    "src/core/game.cpp"
)

# 2. Tell the sandbox where its own headers are
target_include_directories(sandbox PRIVATE "include")

# 3. Link against your Engine library
target_link_libraries(sandbox PRIVATE engine)

# 4. Copy assets to the build directory so the executable can find them
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")