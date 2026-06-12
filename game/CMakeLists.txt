project(game CXX)

# The shipped game runtime — the engine's second front-end besides the editor.
# It is compiled once here; the editor's "File > Export Game..." packages this
# executable together with a project's assets (no compiler needed at export).
add_executable(game "src/main.cpp")

# Windows file icon — exported games inherit it until they ship their own.
if(WIN32)
    target_sources(game PRIVATE "resources/app.rc")
endif()

target_link_libraries(game PRIVATE engine raylib Jolt)
