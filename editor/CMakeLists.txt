project(editor CXX)

# Find all .cpp files in the editor source folder
file(GLOB_RECURSE EDITOR_SOURCES "src/*.cpp")
add_executable(editor ${EDITOR_SOURCES})

# Tell the editor where its own headers are
target_include_directories(editor PRIVATE "include")

# Link the Engine, ImGui, and Raylib
target_link_libraries(editor PRIVATE engine imgui raylib)

# Copy assets AFTER every build (just like we did for the sandbox)
add_custom_command(TARGET editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:editor>/assets"
    COMMENT "Copying editor assets to build directory..."
)