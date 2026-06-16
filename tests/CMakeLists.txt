# Headless tests for the engine (no window / GPU / audio device required).

add_executable(scene_roundtrip_test scene_roundtrip.cpp)

# raylib is PRIVATE-linked into `engine`, so it isn't propagated transitively;
# link it (and Jolt) explicitly to resolve the symbols engine.lib references.
target_link_libraries(scene_roundtrip_test PRIVATE engine raylib Jolt)

add_test(NAME scene_roundtrip COMMAND scene_roundtrip_test)

# Headless animation-evaluator tests (keyframe lerping, easing, JSON lerp).
# Uses the editor's scene_animation.hpp header, hence the extra include dir.
add_executable(animation_eval_test animation_eval.cpp)
target_include_directories(animation_eval_test PRIVATE "${CMAKE_SOURCE_DIR}/editor/include")
target_link_libraries(animation_eval_test PRIVATE engine raylib Jolt)

add_test(NAME animation_eval COMMAND animation_eval_test)

# Headless gameplay-Lua-API tests (Vector3/Color maths, sandbox, Entity bindings).
add_executable(lua_api_test lua_api.cpp)
target_link_libraries(lua_api_test PRIVATE engine raylib Jolt)

add_test(NAME lua_api COMMAND lua_api_test)

# Headless collision-event + raycast tests (Jolt world, no window needed).
add_executable(physics_events_test physics_events.cpp)
target_link_libraries(physics_events_test PRIVATE engine raylib Jolt)

add_test(NAME physics_events COMMAND physics_events_test)

# Dev tool: compiles the raytracer compute shader to validate GLSL edits. Opens a
# brief GL window (compute needs a 4.3 context), so it's not registered with CTest.
add_executable(shader_check shader_check.cpp)
target_link_libraries(shader_check PRIVATE raylib)
