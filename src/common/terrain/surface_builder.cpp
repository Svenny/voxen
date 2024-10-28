#include <voxen/common/terrain/surface_builder.hpp>

#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/qef_solver.hpp>

#include <algorithm>
#include <unordered_map>

namespace voxen::terrain
{

void SurfaceBuilder::buildSurface(Chunk &chunk)
{
	ChunkSurface &surface = chunk.surface();
	surface.clear();

	const ChunkPrimaryData &primary = chunk.primaryData();
	const HermiteDataStorage &hermite_x = primary.hermite_data_x;
	const HermiteDataStorage &hermite_y = primary.hermite_data_y;
	const HermiteDataStorage &hermite_z = primary.hermite_data_z;

	if (hermite_x.empty() && hermite_y.empty() && hermite_z.empty()) {
		// This chunk has no edges - no need to spend any more time
		return;
	}

	struct VertexAccumulator {
		QefSolver3D qef_solver;
		glm::vec3 normal { 0.0f };
		float count = 0.0f;
	};

	std::unordered_map<uint32_t, VertexAccumulator> cell_to_accumulator;

	auto add_point = [&](glm::ivec3 cell_coord, const HermiteDataEntry &entry) {
		constexpr uint32_t KMULT = Config::CHUNK_SIZE + 2u;
		const glm::uvec3 coord(cell_coord + 1);
		const uint32_t key = (coord.x * KMULT + coord.y) * KMULT + coord.z;

		auto &accum = cell_to_accumulator[key];
		glm::vec3 normal = entry.surfaceNormal();
		accum.qef_solver.addPlane(entry.surfacePoint(), normal);
		accum.normal += normal;
		accum.count += 1.0f;
	};

	auto add_edge = [&](const HermiteDataEntry &entry, int axis) {
		int axis2 = (axis + 1) % 3;
		int axis3 = (axis + 2) % 3;

		glm::ivec3 s0(0);
		s0[axis2] -= 1;
		s0[axis3] -= 1;

		glm::ivec3 s1(0);
		s1[axis3] -= 1;

		glm::ivec3 s2(0);
		s2[axis2] -= 1;

		const glm::ivec3 lesser = entry.lesserEndpoint();
		add_point(lesser + s0, entry);
		add_point(lesser + s1, entry);
		add_point(lesser + s2, entry);
		add_point(lesser, entry);
	};

	for (const auto &entry : hermite_x) {
		add_edge(entry, 0);
	}

	for (const auto &entry : hermite_y) {
		add_edge(entry, 1);
	}

	for (const auto &entry : hermite_z) {
		add_edge(entry, 2);
	}

	std::unordered_map<uint32_t, uint32_t> cell_to_vertex_id;

	auto get_vertex_id = [&](glm::ivec3 cell_coord) {
		constexpr uint32_t KMULT = Config::CHUNK_SIZE + 2u;
		const glm::uvec3 coord(cell_coord + 1);
		const uint32_t key = (coord.x * KMULT + coord.y) * KMULT + coord.z;

		auto iter = cell_to_vertex_id.find(key);
		if (iter != cell_to_vertex_id.end()) {
			return iter->second;
		}

		glm::vec3 min_point(cell_coord);
		glm::vec3 max_point(cell_coord + 1);
		bool corner = glm::any(glm::lessThan(cell_coord, glm::ivec3(0)))
			|| glm::any(glm::greaterThanEqual(cell_coord, glm::ivec3(Config::CHUNK_SIZE)));

		auto &accum = cell_to_accumulator[key];
		SurfaceVertex vtx {};
		vtx.position = accum.qef_solver.solve(min_point, max_point);
		vtx.normal = glm::normalize(accum.normal / accum.count);
		vtx.flags = uint8_t(corner ? 1u : 0u);

		uint32_t id = surface.addVertex(vtx);
		cell_to_vertex_id[key] = id;
		return id;
	};

	auto add_triangles = [&](const HermiteDataEntry &entry, int axis) {
		int axis2 = (axis + 1) % 3;
		int axis3 = (axis + 2) % 3;

		glm::ivec3 s0(0);
		s0[axis2] -= 1;
		s0[axis3] -= 1;

		glm::ivec3 s1(0);
		s1[axis3] -= 1;

		glm::ivec3 s2(0);
		s2[axis2] -= 1;

		const glm::ivec3 lesser = entry.lesserEndpoint();
		uint32_t id0 = get_vertex_id(lesser + s0);
		uint32_t id1 = get_vertex_id(lesser + s1);
		uint32_t id2 = get_vertex_id(lesser + s2);
		uint32_t id3 = get_vertex_id(lesser);

		if (std::max({ id0, id1, id2, id3 }) == UINT32_MAX) {
			return;
		}

		if (entry.isLesserEndpointSolid()) {
			surface.addTriangle(id0, id1, id2);
			surface.addTriangle(id1, id3, id2);
		} else {
			surface.addTriangle(id1, id0, id2);
			surface.addTriangle(id3, id1, id2);
		}
	};

	for (const auto &entry : hermite_x) {
		add_triangles(entry, 0);
	}

	for (const auto &entry : hermite_y) {
		add_triangles(entry, 1);
	}

	for (const auto &entry : hermite_z) {
		add_triangles(entry, 2);
	}
}

} // namespace voxen::terrain
