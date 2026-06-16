// Compiles the engine's shaders exactly like the runtime does — the raytracer
// compute shader (as RaytracerSystem::on_start) and the lighting vertex/fragment
// pair (as render::init) — and reports whether they link. Compute shaders need a
// GL 4.3 context, so this briefly opens a tiny window. Run from a directory that
// contains assets/shaders/.
//
// On a GLSL error, rlgl logs the compiler output (with line numbers) to stdout,
// which makes this a fast way to validate shader edits without driving the editor.

#include <raylib.h>
#include <rlgl.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static std::string read_text(const char* path) {
	std::ifstream in(path);
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

int main() {
	SetTraceLogLevel(LOG_ALL);
	InitWindow(96, 96, "shader_check");

	bool ok = true;

	// --- Raytracer compute shader ---
	std::string comp = read_text("assets/shaders/raytracer.comp");
	if (comp.empty()) {
		printf("SHADER_CHECK_FAIL: could not read assets/shaders/raytracer.comp\n");
		ok = false;
	} else {
		unsigned int csId = rlLoadShader(comp.c_str(), RL_COMPUTE_SHADER);
		unsigned int prog = rlLoadShaderProgramCompute(csId);
		printf("SHADER_CHECK raytracer.comp: compiled_id=%u program_id=%u\n", csId, prog);
		if (prog == 0) ok = false;
	}

	// --- Lighting vertex/fragment pair (the forward renderer + shadow sampling) ---
	std::string vs = read_text("assets/shaders/lighting.vs");
	std::string fs = read_text("assets/shaders/lighting.fs");
	if (vs.empty() || fs.empty()) {
		printf("SHADER_CHECK_FAIL: could not read assets/shaders/lighting.vs/.fs\n");
		ok = false;
	} else {
		Shader lighting = LoadShaderFromMemory(vs.c_str(), fs.c_str());
		// raylib falls back to the default shader when compilation/link fails.
		printf("SHADER_CHECK lighting: program_id=%u (default=%u)\n", lighting.id, rlGetShaderIdDefault());
		if (lighting.id == 0 || lighting.id == rlGetShaderIdDefault()) ok = false;
		else UnloadShader(lighting);
	}

	printf(ok ? "SHADER_CHECK_OK\n" : "SHADER_CHECK_FAIL\n");

	CloseWindow();
	return ok ? 0 : 1;
}
