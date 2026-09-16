// Dev tool: benchmarks the path tracer's two-level BVH.
//
// For each scene it reports the build time, the tree size and depth, and the
// average time per primary ray both with the BVH and with an exhaustive search
// over the same primitives. The exhaustive variant runs the engine's own
// traversal on a copy of the tree collapsed into a single leaf, so both variants
// execute identical intersection code — and the tool checks that both return
// the same hits. The editor itself always traces through the BVH.
//
// Timings rather than assertions, so it's not registered with CTest. Build in
// Release and run single-threaded on an idle machine:
//   bvh_bench [model.obj]    (defaults to the demo dragon)

// raylib.h must come before raymath.h, which bvh.hpp pulls in.
#include <raylib.h>
#include <raymath.h>

#include <mini-engine-raylib/render/bvh.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-ecs/registry.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace me::raytracing;
using Clock = std::chrono::steady_clock;

namespace {

	struct RaySet {
		std::vector<Vector3> origins, dirs, invs;
		size_t size() const { return dirs.size(); }
	};

	struct Result {
		std::string scene;
		size_t prims = 0;
		double build_ms = 0.0;
		size_t nodes = 0;
		int depth = 0;
		double bvh_us = 0.0;   // microseconds per ray, BVH
		double flat_us = 0.0;  // microseconds per ray, exhaustive
		size_t checked = 0;    // rays traced by both variants
		size_t agree = 0;      // ... with the same hit/miss and distance
		size_t hits = 0;
	};

	double ms_since(Clock::time_point t0) {
		return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
	}

	// Same guard the raytracer system uses before calling BVH::traverse.
	Vector3 inverse_dir(const Vector3& d) {
		return {
			1.0f / (std::abs(d.x) > 1e-8f ? d.x : 1e-8f),
			1.0f / (std::abs(d.y) > 1e-8f ? d.y : 1e-8f),
			1.0f / (std::abs(d.z) > 1e-8f ? d.z : 1e-8f)
		};
	}

	// Primary rays from an eye `distance` in front of `center` (towards -Z)
	// through a res x res grid spanning center.xy +- half on the plane z = center.z.
	RaySet make_rays(const Vector3& center, float half, float distance, int res) {
		RaySet rays;
		Vector3 eye = { center.x, center.y, center.z - distance };
		float step = 2.0f * half / res;
		for (int y = 0; y < res; ++y) {
			for (int x = 0; x < res; ++x) {
				Vector3 target = { center.x - half + (x + 0.5f) * step, center.y - half + (y + 0.5f) * step, center.z };
				Vector3 dir = Vector3Normalize(Vector3Subtract(target, eye));
				rays.origins.push_back(eye);
				rays.dirs.push_back(dir);
				rays.invs.push_back(inverse_dir(dir));
			}
		}
		return rays;
	}

	std::vector<size_t> every_nth(size_t total, size_t wanted) {
		wanted = std::clamp<size_t>(wanted, 1, total);
		std::vector<size_t> idx;
		size_t stride = total / wanted;
		for (size_t i = 0; i < total && idx.size() < wanted; i += stride) idx.push_back(i);
		return idx;
	}

	// Average seconds per ray over `rays`, repeating passes for at least min_seconds.
	template <typename Trace>
	double seconds_per_ray(const std::vector<size_t>& rays, Trace&& trace, double min_seconds = 0.3) {
		size_t traced = 0;
		double elapsed = 0.0;
		auto t0 = Clock::now();
		do {
			for (size_t i : rays) trace(i);
			traced += rays.size();
			elapsed = std::chrono::duration<double>(Clock::now() - t0).count();
		} while (elapsed < min_seconds);
		return elapsed / traced;
	}

	// An evenly spread ray subset whose exhaustive tracing takes about budget_seconds.
	template <typename Trace>
	std::vector<size_t> budgeted_subset(size_t total, Trace&& trace, double budget_seconds) {
		std::vector<size_t> probe = every_nth(total, 8);
		auto t0 = Clock::now();
		for (size_t i : probe) trace(i);
		double per_ray = std::chrono::duration<double>(Clock::now() - t0).count() / probe.size();
		return every_nth(total, static_cast<size_t>(budget_seconds / std::max(per_ray, 1e-9)));
	}

	// Both node types expose is_leaf() and left_child (right child = left + 1).
	template <typename Node>
	int tree_depth(const std::vector<Node>& nodes, uint32_t i = 0) {
		if (nodes.empty()) return 0;
		const Node& n = nodes[i];
		if (n.is_leaf()) return 1;
		return 1 + std::max(tree_depth(nodes, n.left_child), tree_depth(nodes, n.left_child + 1));
	}

	// ------------------------------------------------------------------
	// BLAS: one triangle mesh
	// ------------------------------------------------------------------

	struct MeshData {
		std::vector<float> vertices; // non-indexed: 3 vertices per triangle
		size_t triangles() const { return vertices.size() / 9; }
	};

	void push_triangle(MeshData& mesh, const Vector3& a, const Vector3& b, const Vector3& c) {
		for (const Vector3* v : { &a, &b, &c }) {
			mesh.vertices.push_back(v->x);
			mesh.vertices.push_back(v->y);
			mesh.vertices.push_back(v->z);
		}
	}

	// Unit UV sphere with roughly `target` triangles.
	MeshData make_sphere(size_t target) {
		int n = std::max(4, static_cast<int>(std::lround(std::sqrt(target / 2.0))));
		auto point = [n](int ring, int slice) {
			float theta = PI * ring / n;
			float phi = 2.0f * PI * slice / n;
			return Vector3{ std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi) };
		};
		MeshData mesh;
		for (int r = 0; r < n; ++r) {
			for (int s = 0; s < n; ++s) {
				Vector3 p00 = point(r, s), p01 = point(r, s + 1), p10 = point(r + 1, s), p11 = point(r + 1, s + 1);
				if (r > 0) push_triangle(mesh, p00, p10, p01);       // skip the degenerate cap at the north pole
				if (r < n - 1) push_triangle(mesh, p01, p10, p11);   // ... and at the south pole
			}
		}
		return mesh;
	}

	// Minimal Wavefront reader (positions + faces, polygons fanned). Avoids raylib's
	// LoadModel, which uploads to the GPU and so needs a window.
	bool load_obj(const std::string& path, MeshData& mesh) {
		std::ifstream file(path);
		if (!file) return false;
		std::vector<Vector3> positions;
		std::vector<std::vector<int>> faces;
		std::string line;
		while (std::getline(file, line)) {
			if (line.size() < 2 || line[1] != ' ') continue;
			std::istringstream in(line.substr(2));
			if (line[0] == 'v') {
				Vector3 p{};
				in >> p.x >> p.y >> p.z;
				positions.push_back(p);
			} else if (line[0] == 'f') {
				std::vector<int> face;
				std::string token;
				while (in >> token) face.push_back(std::stoi(token.substr(0, token.find('/'))));
				faces.push_back(std::move(face));
			}
		}
		auto at = [&](int i) { return positions[i > 0 ? i - 1 : positions.size() + i]; };
		for (const auto& face : faces)
			for (size_t k = 1; k + 1 < face.size(); ++k)
				push_triangle(mesh, at(face[0]), at(face[k]), at(face[k + 1]));
		return mesh.triangles() > 0;
	}

	Result bench_blas(const std::string& scene, const MeshData& mesh) {
		::Mesh rl_mesh{};
		rl_mesh.vertexCount = static_cast<int>(mesh.vertices.size() / 3);
		rl_mesh.triangleCount = static_cast<int>(mesh.triangles());
		rl_mesh.vertices = const_cast<float*>(mesh.vertices.data()); // read-only use
		::Model model{};
		model.transform = MatrixIdentity();
		model.meshCount = 1;
		model.meshes = &rl_mesh;

		Result r;
		r.scene = scene;
		r.prims = mesh.triangles();

		TriangleBVH bvh;
		auto t0 = Clock::now();
		bvh.build_from_model(model);
		r.build_ms = ms_since(t0);
		r.nodes = bvh.m_nodes.size();
		r.depth = tree_depth(bvh.m_nodes);

		TriangleBVH flat; // same triangles in a single leaf => exhaustive search
		flat.local_bounds = bvh.local_bounds;
		flat.m_triangles = bvh.m_triangles;
		flat.m_triangle_indices = bvh.m_triangle_indices;
		TriangleNode leaf;
		leaf.bounds = bvh.m_nodes[0].bounds;
		leaf.triangle_count = static_cast<uint32_t>(flat.m_triangle_indices.size());
		flat.m_nodes = { leaf };

		const AABB& b = bvh.local_bounds;
		Vector3 size = Vector3Subtract(b.max, b.min);
		RaySet rays = make_rays(b.centroid(), 0.55f * std::max(size.x, size.y), 2.5f * std::max({ size.x, size.y, size.z }), 128);

		auto trace = [&](const TriangleBVH& tree, size_t i, float& t) {
			Vector3 normal;
			return tree.intersect(rays.origins[i], rays.dirs[i], t, normal);
		};

		std::vector<size_t> all(rays.size());
		std::iota(all.begin(), all.end(), size_t{ 0 });
		r.bvh_us = 1e6 * seconds_per_ray(all, [&](size_t i) { float t; trace(bvh, i, t); });

		std::vector<size_t> subset = budgeted_subset(rays.size(), [&](size_t i) { float t; trace(flat, i, t); }, 1.0);
		r.flat_us = 1e6 * seconds_per_ray(subset, [&](size_t i) { float t; trace(flat, i, t); }, 0.0);

		for (size_t i : subset) {
			float ta = 0.0f, tb = 0.0f;
			bool ha = trace(bvh, i, ta);
			bool hb = trace(flat, i, tb);
			++r.checked;
			if (ha) ++r.hits;
			if (ha == hb && (!ha || std::abs(ta - tb) <= 1e-4f * std::max(1.0f, ta))) ++r.agree;
		}
		return r;
	}

	// ------------------------------------------------------------------
	// TLAS: ECS entities (analytic spheres and cubes)
	// ------------------------------------------------------------------

	Result bench_tlas(size_t count, std::mt19937& rng) {
		using namespace me::components;
		me::Registry reg;

		// Constant density: ~27 cubic units per object, random position and orientation.
		float side = 3.0f * std::cbrt(static_cast<float>(count));
		std::uniform_real_distribution<float> coord(-0.5f * side, 0.5f * side);
		std::uniform_real_distribution<float> angle(0.0f, 2.0f * PI);
		for (size_t i = 0; i < count; ++i) {
			TransformComponent t;
			t.position = { coord(rng), coord(rng), coord(rng) };
			t.scale = { 0.5f, 0.5f, 0.5f };
			t.rotation_quat = QuaternionFromEuler(angle(rng), angle(rng), angle(rng));
			// Scale * Rotation * Translation, as TransformSystem builds it
			t.model_matrix = MatrixMultiply(MatrixMultiply(MatrixScale(t.scale.x, t.scale.y, t.scale.z),
				QuaternionToMatrix(t.rotation_quat)), MatrixTranslate(t.position.x, t.position.y, t.position.z));

			Shape3DComponent shape;
			shape.type = (i % 2 == 0) ? Shape3DComponent::Sphere : Shape3DComponent::Cube;

			auto e = reg.create_entity();
			e.add_component(t);
			e.add_component(shape);
		}

		Result r;
		r.scene = "objects";
		r.prims = count;

		BVH bvh;
		auto t0 = Clock::now();
		bvh.build(reg);
		r.build_ms = ms_since(t0);
		r.nodes = bvh.m_nodes.size();
		r.depth = tree_depth(bvh.m_nodes);

		BVH flat; // same primitives in a single leaf => exhaustive search
		flat.m_prims = bvh.m_prims;
		flat.m_prim_indices = bvh.m_prim_indices;
		BVHNode leaf;
		leaf.bounds = bvh.m_nodes[0].bounds;
		leaf.triangle_count = static_cast<uint32_t>(flat.m_prim_indices.size());
		flat.m_nodes = { leaf };

		RaySet rays = make_rays({ 0.0f, 0.0f, 0.0f }, 0.5f * side, 1.5f * side, 128);
		auto trace = [&](const BVH& tree, size_t i) {
			return tree.traverse(rays.origins[i], rays.dirs[i], rays.invs[i], FLT_MAX, reg);
		};

		std::vector<size_t> all(rays.size());
		std::iota(all.begin(), all.end(), size_t{ 0 });
		r.bvh_us = 1e6 * seconds_per_ray(all, [&](size_t i) { trace(bvh, i); });

		std::vector<size_t> subset = budgeted_subset(rays.size(), [&](size_t i) { trace(flat, i); }, 1.0);
		r.flat_us = 1e6 * seconds_per_ray(subset, [&](size_t i) { trace(flat, i); }, 0.0);

		for (size_t i : subset) {
			auto a = trace(bvh, i);
			auto b = trace(flat, i);
			++r.checked;
			if (a) ++r.hits;
			bool same = a.has_value() == b.has_value()
				&& (!a || std::abs(a->hit_distance - b->hit_distance) <= 1e-4f * std::max(1.0f, a->hit_distance));
			if (same) ++r.agree;
		}
		return r;
	}

	// ------------------------------------------------------------------
	// Output
	// ------------------------------------------------------------------

	void print_header(const char* title) {
		std::printf("\n%s\n", title);
		std::printf("%-8s %9s %11s %9s %6s %13s %14s %9s %13s %6s\n",
			"scene", "prims", "build (ms)", "nodes", "depth", "BVH (us/ray)", "flat (us/ray)", "speedup", "same hits", "hit %");
	}

	void print_row(const Result& r) {
		std::printf("%-8s %9zu %11.2f %9zu %6d %13.4f %14.4f %8.1fx %6zu/%-6zu %5.1f\n",
			r.scene.c_str(), r.prims, r.build_ms, r.nodes, r.depth, r.bvh_us, r.flat_us,
			r.flat_us / r.bvh_us, r.agree, r.checked, r.checked ? 100.0 * r.hits / r.checked : 0.0);
		if (r.depth >= 60) // CPU (bvh.hpp) and GPU (raytracer.comp) traversals use 64-entry stacks
			std::printf("         WARNING: depth %d is close to the 64-entry traversal stack\n", r.depth);
		std::fflush(stdout);
	}

} // namespace

int main(int argc, char** argv) {
	std::string model_path = argc > 1 ? argv[1] : ME_DEMO_MODEL;

#ifdef NDEBUG
	std::printf("bvh_bench: Release build, single thread, 128x128 primary rays per scene\n");
#else
	std::printf("bvh_bench: DEBUG build, timings are not representative (use x64-release)\n");
#endif

	print_header("BLAS - triangle BVH over one mesh");
	for (size_t n : { 1000, 4000, 16000, 64000, 256000, 1024000 })
		print_row(bench_blas("sphere", make_sphere(n)));

	MeshData dragon;
	if (load_obj(model_path, dragon)) print_row(bench_blas("dragon", dragon));
	else std::printf("(model not found: %s)\n", model_path.c_str());

	print_header("TLAS - object BVH over ECS entities (half spheres, half cubes)");
	std::mt19937 rng(1234);
	for (size_t n : { 16, 64, 256, 1024, 4096, 16384 })
		print_row(bench_tlas(n, rng));

	return 0;
}
