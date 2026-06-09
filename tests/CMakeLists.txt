# Headless tests for the engine (no window / GPU / audio device required).

add_executable(scene_roundtrip_test scene_roundtrip.cpp)

# raylib is PRIVATE-linked into `engine`, so it isn't propagated transitively;
# link it (and Jolt) explicitly to resolve the symbols engine.lib references.
target_link_libraries(scene_roundtrip_test PRIVATE engine raylib Jolt)

add_test(NAME scene_roundtrip COMMAND scene_roundtrip_test)
