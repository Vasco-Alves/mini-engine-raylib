#pragma once

namespace me {

    struct EngineConfig {
        int width = 1280, height = 720;
        const char *title = "MiniEngine";
        bool vsync = false;
        int targetFps = 0;
    };

    bool Init(const EngineConfig &cfg = {});

    bool Update(); // returns false if the app should exit

    void BeginFrame(); // starts rendering

    void EndFrame(); // ends rendering and presents

    void Shutdown();

    // Optional convenience for prototypes (kept, but *you* will use the split):
    inline bool Tick() {
        if (!Update())
            return false;
            
        BeginFrame();
        EndFrame();

        return true;
    }

} // namespace me
