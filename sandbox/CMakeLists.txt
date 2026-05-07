# 1. Create the Executable
add_executable(sandbox 
    "src/main.cpp"
    "src/core/game.cpp"
    "src/scene/testscene.cpp"
)

# file(GLOB_RECURSE SANDBOX_SOURCES "src/*.cpp")
# add_executable(sandbox ${SANDBOX_SOURCES})

# 2. Tell the sandbox where its own headers are
target_include_directories(sandbox PRIVATE "include")

# 3. Link against your Engine library
target_link_libraries(sandbox PRIVATE engine)

# 4. Copy assets after every build
add_custom_command(TARGET sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:sandbox>/assets"
    COMMENT "Copying sandbox assets to build directory..."
)