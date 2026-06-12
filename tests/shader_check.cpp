// Compiles the raytracer compute shader exactly like RaytracerSystem::on_start does,
// and reports whether it links. Compute shaders need a GL 4.3 context, so this briefly
// opens a tiny window. Run from a directory that contains assets/shaders/raytracer.comp.
//
// On a GLSL error, rlgl logs the compiler output (with line numbers) to stdout, which
// makes this a fast way to validate shader edits without driving the full editor.

#include <raylib.h>
#include <rlgl.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main() {
	SetTraceLogLevel(LOG_ALL);
	InitWindow(96, 96, "shader_check");

	std::ifstream in("assets/shaders/raytracer.comp");
	std::stringstream ss;
	ss << in.rdbuf();
	std::string src = ss.str();
	if (src.empty()) {
		printf("SHADER_CHECK_FAIL: could not read assets/shaders/raytracer.comp\n");
		CloseWindow();
		return 2;
	}

	unsigned int csId = rlLoadShader(src.c_str(), RL_COMPUTE_SHADER);
	unsigned int prog = rlLoadShaderProgramCompute(csId);

	printf("SHADER_CHECK: compiled_id=%u program_id=%u\n", csId, prog);
	printf(prog != 0 ? "SHADER_CHECK_OK\n" : "SHADER_CHECK_FAIL\n");

	CloseWindow();
	return prog != 0 ? 0 : 1;
}
