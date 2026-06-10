project(editor CXX)

# Explicit source list — preferred over file(GLOB), which doesn't re-run when a new
# file is added (so a new .cpp would be silently skipped until the next re-configure).
set(EDITOR_SOURCES
    "src/main.cpp"
    "src/core/editor_app.cpp"
    "src/panels/console_panel.cpp"
    "src/panels/content_browser_panel.cpp"
    "src/panels/inspector_panel.cpp"
    "src/panels/project_hub_panel.cpp"
    "src/panels/scene_hierarchy_panel.cpp"
    "src/panels/viewport_panel.cpp"
    "src/utils/file_dialogs.cpp"
    "${CMAKE_SOURCE_DIR}/vendor/imguizmo/ImGuizmo.cpp"
)

add_executable(editor ${EDITOR_SOURCES})

# Tell the editor where its own headers and ImGuizmo are
target_include_directories(editor PRIVATE 
    "include"
    "${CMAKE_SOURCE_DIR}/vendor/imguizmo"
)

# Link the Engine, ImGui, and Raylib
target_link_libraries(editor PRIVATE engine imgui raylib Jolt)

# Copy assets after every build
add_custom_command(TARGET editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:editor>/assets"
    COMMENT "Copying editor assets to build directory..."
)