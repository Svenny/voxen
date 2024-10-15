#include <voxen/common/terrain/surface.hpp>

#include <cassert>
#include <limits>

namespace voxen::terrain
{

void ChunkSurface::clear() noexcept
{
	m_vertices.clear();
	m_indices.clear();
	m_aabb = Aabb();
}

uint32_t ChunkSurface::addVertex(const SurfaceVertex &vertex)
{
	m_vertices.emplace_back(vertex);
	m_aabb.includePoint(vertex.position);
	return static_cast<uint32_t>(m_vertices.size() - 1);
}

void ChunkSurface::addTriangle(uint32_t a, uint32_t b, uint32_t c)
{
	assert(std::max({ a, b, c }) < numVertices());

	if (a == b || b == c || a == c) {
		// Skip degenerate triangles
		return;
	}

	voxel_t m1 = m_vertices[a].materials[0];
	voxel_t m2 = m_vertices[b].materials[0];
	voxel_t m3 = m_vertices[c].materials[0];

	if (m1 == m2 && m2 == m3) {
		m_indices.emplace_back(a);
		m_indices.emplace_back(b);
		m_indices.emplace_back(c);
		return;
	}

	SurfaceVertex v1 = m_vertices[a];
	SurfaceVertex v2 = m_vertices[b];
	SurfaceVertex v3 = m_vertices[c];

	v1.materials[0] = v2.materials[0] = v3.materials[0] = m1;
	v1.materials[1] = v2.materials[1] = v3.materials[1] = m2;
	v1.materials[2] = v2.materials[2] = v3.materials[2] = m3;

	v1.flags |= 0b000;
	v2.flags |= 0b010;
	v3.flags |= 0b100;

	a = addVertex(v1);
	b = addVertex(v2);
	c = addVertex(v3);

	m_indices.emplace_back(a);
	m_indices.emplace_back(b);
	m_indices.emplace_back(c);
}

void ChunkSurface::relaxMaterials()
{
	return;
	/*struct Neighborhood {
		std::array<uint32_t, 12> ids;
		uint32_t num_ids = 0;
	};

	const size_t n = m_vertices.size();
	extras::dyn_array<Neighborhood> neighbors(n);

	auto addEdge = [&](uint32_t a, uint32_t b) {
		auto &n1 = neighbors[a];
		auto &n2 = neighbors[b];

		auto aend = n1.ids.begin() + n1.num_ids;
		auto bend = n2.ids.begin() + n2.num_ids;

		auto ait = std::find(n1.ids.begin(), aend, b);
		if (ait == aend) {
			assert(n1.num_ids < 12);
			n1.ids[n1.num_ids++] = b;
		}

		auto bit = std::find(n2.ids.begin(), bend, a);
		if (bit == bend) {
			assert(n2.num_ids < 12);
			n2.ids[n2.num_ids++] = a;
		}
	};

	for (size_t i = 0; i < m_indices.size(); i += 3) {
		uint32_t a = m_indices[i];
		uint32_t b = m_indices[i + 1];
		uint32_t c = m_indices[i + 2];

		addEdge(a, b);
		addEdge(a, c);
		addEdge(b, c);
	}

	extras::dyn_array<uint32_t> last_visitor(n, UINT32_MAX);
	std::queue<std::tuple<uint32_t, int, float>> q;
	std::array<float, 256> fucks;

	for (size_t i = 0; i < n; i++) {
		fucks.fill(0.0f);

		q.emplace(uint32_t(i), 0, 1.0f);

		while (!q.empty()) {
			auto [v, depth, weight] = q.front();
			q.pop();

			if (depth >= 3 || last_visitor[v] == i) {
				continue;
			}

			last_visitor[v] = uint32_t(i);

			fucks[m_vertices[v].materials[0]] += float(255 - m_vertices[v].mat_ratio) / 255.0f;
			fucks[m_vertices[v].materials[1]] += float(m_vertices[v].mat_ratio) / 255.0f;

			for (uint32_t j = 0; j < neighbors[v].num_ids; j++) {
				q.emplace(neighbors[v].ids[j], depth + 1, weight * 0.1f);
			}
		}

		voxel_t mats[2] = { 0, 0 };
		float weights[2] = { 0.0f, 0.0f };

		for (size_t j = 0; j < 3; j++) {
			auto mx = std::max_element(fucks.begin(), fucks.end());
			if (*mx > 0.0f) {
				mats[j] = voxel_t(mx - fucks.begin());
				weights[j] = *mx;
				*mx = -1.0f;
			}
		}

		m_vertices[i].materials[0] = mats[0];
		m_vertices[i].materials[1] = mats[1];

		float ratio = weights[1] / (weights[0] + weights[1]);
		m_vertices[i].mat_ratio = uint8_t(ratio * 255.0f);
	}*/
}

} // namespace voxen::terrain
