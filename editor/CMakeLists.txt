project(editor CXX)

set(EDITOR_SOURCES
    "src/main.cpp"
    "src/core/editor_app.cpp"
    "src/core/editor_offline_render.cpp"
    "src/panels/animation_panel.cpp"
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

# The product is "Mini Engine"; the editor ships as MiniEngine.exe.
set_target_properties(editor PROPERTIES OUTPUT_NAME "MiniEngine")

# Windows file icon (the window/taskbar icon is set at runtime from assets/icon.png).
if(WIN32)
    target_sources(editor PRIVATE "resources/app.rc")
endif()

# Tell the editor where its own headers and ImGuizmo are
target_include_directories(editor PRIVATE 
    "include"
    "${CMAKE_SOURCE_DIR}/vendor/imguizmo"
)

# Link the Engine, ImGui, and Raylib
target_link_libraries(editor PRIVATE engine imgui raylib Jolt)

# Copy runtime assets next to the executable after every build:
#  - engine/assets: shaders (owned by the engine, shared by any front-end)
#  - editor/assets: editor-only assets (shipped default layouts)
add_custom_command(TARGET editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/engine/assets"
    "$<TARGET_FILE_DIR:editor>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:editor>/assets"
    COMMENT "Copying engine + editor assets to build directory..."
)