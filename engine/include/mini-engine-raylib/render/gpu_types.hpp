#pragma once
#include <raylib.h>
#include <cstdint>

namespace me::render::gpu {

	// 1. The GPU Triangle (Aligned to 16-byte boundaries)
	struct GPUTriangle {
		Vector3 v0;
		float   pad0; // GPU forces 16 bytes per vector
		Vector3 v1;
		float   pad1;
		Vector3 v2;
		float   pad2;
	};

	// 2. The GPU BVH Node (32 bytes total per node)
	struct GPUNode {
		Vector3 bounds_min;
		uint32_t left_child; // Or primitive/triangle index if leaf

		Vector3 bounds_max;
		uint32_t count;      // 0 = internal node, > 0 = leaf node
	};

	// 3. The GPU Material (Aligned to 48 bytes — pads keep the std430 stride exact)
	struct GPUMaterial {
		Vector3 base_color;
		float   roughness;

		float   metallic;
		float   emission;
		float   transmission;
		float   ior;

		float   tint_strength; // Beer–Lambert density multiplier (0 = always clear)
		float   pad0;
		float   pad1;
		float   pad2;
	};

	// 4. The GPU Primitive (For Spheres, Planes, and linking Models)
	struct GPUPrimitive {
		int type;             // 0=Sphere, 1=Plane, 2=Cube, 3=Model
		int material_index;   // Index into the GPUMaterial array
		int blas_root_index;  // If Model: where does its BVH tree start in the Node Array?
		float pad0;

		// Pre-computed matrices for exact OBB and Model ray transformations
		Matrix inverse_model;
		Matrix normal_matrix;
	};

	// 5. Point light (position + color + intensity)
	struct GPUPointLight {
		Vector3 position;
		float   radius;    // world-space light size → soft shadows
		Vector3 color;     // linear-space
		float   intensity;
	};

	// 6. Directional light (direction is TOWARD the light, pre-computed on CPU)
	struct GPUDirLight {
		Vector3 direction; // normalized, toward light
		float   cos_angular; // cos(half-angle) of the light's angular size → soft shadows
		Vector3 color;     // linear-space
		float   intensity;
	};

	// 7. Emissive area light (sphere-approximated) used for Next Event Estimation.
	// Every object with a non-zero emission_power becomes one of these so it can
	// illuminate the rest of the scene directly instead of only via random bounces.
	struct GPUEmitter {
		Vector3 position;  // world-space center
		float   radius;    // bounding-sphere radius
		Vector3 emission;  // linear-space radiance (base_color * emission_power)
		float   pad0;
	};

} // namespace me::render::gpu