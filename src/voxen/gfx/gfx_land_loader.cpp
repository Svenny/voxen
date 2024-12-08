#include <voxen/gfx/gfx_land_loader.hpp>

#include <voxen/common/world_state.hpp>
#include <voxen/gfx/gfx_system.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_dma_system.hpp>
#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/gfx/vk/vk_mesh_streamer.hpp>
#include <voxen/land/land_messages.hpp>
#include <voxen/land/land_service.hpp>
#include <voxen/land/land_utils.hpp>
#include <voxen/svc/messaging_service.hpp>
#include <voxen/svc/service_locator.hpp>
#include <voxen/util/concentric_octahedra_walker.hpp>
#include <voxen/util/log.hpp>

#include <extras/bitset.hpp>
#include <extras/defer.hpp>

namespace voxen::gfx
{

namespace
{

constexpr UID LAND_LOADER_DOMAIN_UID("0a7ac6a1-18693cb0-21c2e1bb-912579d5");

constexpr uint32_t LOAD_BOX_DISTANCE = 5;

bool wantTrueGeometry(double sq_distance)
{
	return sq_distance < 100.0 * 100;
}

} // namespace

class detail::LandLoaderImpl {
public:
	using DrawCommand = LandLoader::DrawCommand;
	using DrawList = LandLoader::DrawList;

	LandLoaderImpl(GfxSystem &gfx, svc::ServiceLocator &svc) : m_gfx(gfx), dev(*gfx.device())
	{
		// HELL YEAH!!!
		UID my_uid = UID::generateRandom();
		m_message_sender = svc.requestService<svc::MessagingService>().createSender(my_uid);
	}

	~LandLoaderImpl() {}

	void onNewState(const WorldState &state)
	{
		const auto &new_land = state.landState();
		last_known_pseudo_chunk_surface_table = new_land.pseudo_chunk_surface_table;
	}

	bool makeRenderListGeometry(DrawList &dlist, glm::ivec3 chunk_base)
	{
		(void) dlist;
		(void) chunk_base;
		// True geometry, add it or we can't draw the chunk at all
		// TODO: geometry is not yet implemented
		return false;
	}

	bool requestImpostorFaces(land::ChunkKey key, DrawCommand &dcmd)
	{
		auto *table_item = last_known_pseudo_chunk_surface_table.find(key);

		if (!table_item) {
			// Unknown chunk
			return false;
		}

		if (!table_item->hasValue() || table_item->value().empty()) {
			// Known empty chunk
			dcmd = {};
			return true;
		}

		const UID key_uid = Hash::keyToUid(LAND_LOADER_DOMAIN_UID, key.packed());

		vk::MeshStreamer &streamer = *m_gfx.meshStreamer();
		vk::MeshStreamer::MeshInfo mesh_info;
		streamer.queryMesh(key_uid, mesh_info);

		const int64_t latest_version = static_cast<int64_t>(table_item->version());

		if (std::max(mesh_info.ready_version, mesh_info.pending_version) < latest_version) {
			// New chunk data version available, enqueue its upload
			vk::MeshStreamer::MeshAdd mesh_add;
			mesh_add.version = latest_version;

			const land::PseudoChunkSurface &surface = table_item->value();

			mesh_add.substreams[0].data = surface.vertexPositions();
			mesh_add.substreams[0].num_elements = surface.numVertices();
			mesh_add.substreams[0].element_size = sizeof(land::PseudoSurfaceVertexPosition);

			mesh_add.substreams[1].data = surface.vertexAttributes();
			mesh_add.substreams[1].num_elements = surface.numVertices();
			mesh_add.substreams[1].element_size = sizeof(land::PseudoSurfaceVertexAttributes);

			mesh_add.substreams[2].data = surface.indices();
			mesh_add.substreams[2].num_elements = surface.numIndices();
			mesh_add.substreams[2].element_size = sizeof(uint16_t);

			streamer.addMesh(key_uid, mesh_add);
		}

		if (mesh_info.ready_version < 0) {
			return false;
		}

		// Have some data available

		dcmd.index_buffer = mesh_info.substreams[2].vk_buffer;
		dcmd.first_index = mesh_info.substreams[2].first_element;
		dcmd.num_indices = mesh_info.substreams[2].num_elements;

		dcmd.pos_data_address = mesh_info.substreams[0].buffer_gpu_address
			+ mesh_info.substreams[0].first_element * sizeof(land::PseudoSurfaceVertexPosition);
		dcmd.attrib_data_address = mesh_info.substreams[1].buffer_gpu_address
			+ mesh_info.substreams[1].first_element * sizeof(land::PseudoSurfaceVertexAttributes);

		dcmd.chunk_base_x = key.x;
		dcmd.chunk_base_y = key.y;
		dcmd.chunk_base_z = key.z;
		dcmd.chunk_lod = key.scale_log2;

		return true;
	}

	bool makeRenderListImpostor(const glm::dvec3 &viewpoint, DrawList &dlist, const glm::ivec3 chunk_base,
		const uint8_t level)
	{
		const auto &lod_box = m_chunk_ticket_boxes[level];
		if (chunk_base.x < lod_box.begin.x || chunk_base.x >= lod_box.end.x || chunk_base.y < lod_box.begin.y
			|| chunk_base.y >= lod_box.end.y || chunk_base.z < lod_box.begin.z || chunk_base.z >= lod_box.end.z) {
			// Out of render area for this LOD
			return false;
		}

		auto try_finer_level = [&]() {
			const uint8_t n = level - 1;
			const int32_t k = 1 << n;

			bool res = makeRenderListImpostor(viewpoint, dlist, chunk_base, n);
			res &= makeRenderListImpostor(viewpoint, dlist, chunk_base + glm::ivec3(k, 0, 0), n);
			res &= makeRenderListImpostor(viewpoint, dlist, chunk_base + glm::ivec3(0, k, 0), n);
			res &= makeRenderListImpostor(viewpoint, dlist, chunk_base + glm::ivec3(0, 0, k), n);
			res &= makeRenderListImpostor(viewpoint, dlist, chunk_base + glm::ivec3(k, k, 0), n);
			res &= makeRenderListImpostor(viewpoint, dlist, chunk_base + glm::ivec3(0, k, k), n);
			res &= makeRenderListImpostor(viewpoint, dlist, chunk_base + glm::ivec3(k, 0, k), n);
			res &= makeRenderListImpostor(viewpoint, dlist, chunk_base + glm::ivec3(k, k, k), n);

			return res;
		};

		// Some of the finer ones might be missing but we can substitute it with the current level.
		// Remember rlist position and prepare to unwind it upon failure.
		//auto rewind_position = ptrdiff_t(rlist.size());

		// First try finer LODs. Might seem like overkill but
		// out-of-area check above should eliminate most of the work.
		//if (level != 0 && try_finer_level()) {
		//	return true;
		//}

		if (level == 0) {
			const glm::dvec3 center = glm::dvec3(chunk_base) * land::Consts::CHUNK_SIZE_METRES
				+ 0.5 * land::Consts::CHUNK_SIZE_METRES;
			const glm::dvec3 view_dir = viewpoint - center;

			// At short distances, draw true geometry instead of impostors
			const double sq_distance = glm::dot(view_dir, view_dir);
			if (wantTrueGeometry(sq_distance) && makeRenderListGeometry(dlist, chunk_base)) {
				return true;
			}
		}

		land::ChunkKey key(chunk_base, level);

		DrawCommand dcmd;
		bool have_current_data = requestImpostorFaces(key, dcmd);

		if (!have_current_data) {
			// We can't fully substitute a missing finer level.
			// If a certain chunk is missing all the way down to the geometry, there will be
			// a non-rendered hole in its place (there is not enough information to draw it).
			//
			// Should occur only during the initial loading and in some brief transients.
			return false;
		}

		auto rewind_position = ptrdiff_t(dlist.size());

		if (level != 0 && try_finer_level()) {
			return true;
		}

		if (dcmd.num_indices == 0) {
			// Known empty chunk
			return true;
		}

		// Finer levels will have Z-fighting or other issues from overlap
		// with the coarser one, so remove potentially added commands.
		dlist.erase(dlist.begin() + rewind_position, dlist.end());
		dlist.emplace_back(dcmd);
		return true;
	}

	void makeDrawList(const glm::dvec3 &viewpoint, DrawList &dlist)
	{
		const uint32_t last_lod = land::Consts::NUM_LOD_SCALES - 1;

		// Request land service to load chunks in render area
		{
			// If N = CHUNK_SIZE_METRES
			// ...
			// [-N;0) - chunk -1
			// [0;N) - chunk 0
			// [N;2N) - chunk 1
			// ...
			// So to get chunk coordinate, divide by N and round down.

			for (uint32_t lod = 0; lod <= last_lod; lod++) {
				double side_step = double(LOAD_BOX_DISTANCE << lod);
				glm::dvec3 box_lo = viewpoint / land::Consts::CHUNK_SIZE_METRES - side_step;
				glm::dvec3 box_hi = viewpoint / land::Consts::CHUNK_SIZE_METRES + side_step;

				glm::ivec3 lo_coord = glm::ivec3(glm::floor(box_lo));
				glm::ivec3 hi_coord = glm::ivec3(glm::ceil(box_hi));

				// Align to two times the scaled chunk boundaries, basically to the parent LOD grid.
				// This gives a nice property: for every chunk key with LOD > 0 either this key
				// or all of its 8 children are in the renderable set. So we can simplify mesh
				// selection logic: if surfaces for all children are available, take those and stop,
				// otherwise
				//
				// Masking works equally well for negative numbers.
				const int32_t round_add = (1 << (lod + 1)) - 1;
				const int32_t round_mask = ~round_add;

				// Align down
				lo_coord = lo_coord & round_mask;
				// Align up
				hi_coord = (hi_coord + round_add) & round_mask;

				land::ChunkTicketBoxArea wanted_box {
					.begin = land::ChunkKey(lo_coord, lod),
					.end = land::ChunkKey(hi_coord, lod),
				};

				if (m_chunk_tickets[lod].valid()) {
					if (m_chunk_ticket_boxes[lod] != wanted_box) {
						m_chunk_tickets[lod].adjustAsync(wanted_box);
						m_chunk_ticket_boxes[lod] = wanted_box;
					}
				} else if (m_chunk_ticket_requests[lod].valid()) {
					auto status = m_chunk_ticket_requests[lod].status();
					if (status != svc::RequestStatus::Pending) {
						assert(status == svc::RequestStatus::Complete);

						m_chunk_tickets[lod] = std::move(m_chunk_ticket_requests[lod].payload().ticket);
						assert(m_chunk_tickets[lod].valid());

						m_chunk_ticket_requests[lod].reset();
					}
				} else {
					m_chunk_ticket_boxes[lod] = wanted_box;
					m_chunk_ticket_requests[lod]
						= m_message_sender.requestWithHandle<land::ChunkTicketRequestMessage>(land::LandService::SERVICE_UID,
							wanted_box);
				}
			}
		}

		dlist.clear();

		const land::ChunkKey lo = m_chunk_ticket_boxes[last_lod].begin;
		const land::ChunkKey hi = m_chunk_ticket_boxes[last_lod].end;
		const int32_t step = lo.scaleMultiplier();

		for (int64_t y = lo.y; y < hi.y; y += step) {
			for (int64_t x = lo.x; x < hi.x; x += step) {
				for (int64_t z = lo.z; z < hi.z; z += step) {
					makeRenderListImpostor(viewpoint, dlist, glm::ivec3(x, y, z), lo.scale_log2);
				}
			}
		}
	}

	GfxSystem &m_gfx;
	vk::Device &dev;
	svc::MessageSender m_message_sender;

	land::LandState::PseudoChunkSurfaceTable last_known_pseudo_chunk_surface_table;

	land::ChunkTicketBoxArea m_chunk_ticket_boxes[land::Consts::NUM_LOD_SCALES];
	land::ChunkTicket m_chunk_tickets[land::Consts::NUM_LOD_SCALES];
	svc::RequestHandle<land::ChunkTicketRequestMessage> m_chunk_ticket_requests[land::Consts::NUM_LOD_SCALES];
};

LandLoader::LandLoader(GfxSystem &gfx, svc::ServiceLocator &svc) : m_impl(gfx, svc) {}

LandLoader::~LandLoader() noexcept = default;

void LandLoader::onNewState(const WorldState &state)
{
	m_impl->onNewState(state);
}

void LandLoader::makeDrawList(const glm::dvec3 &viewpoint, DrawList &dlist)
{
	if (dlist.empty()) {
		// fook
		//return;
	}

	m_impl->makeDrawList(viewpoint, dlist);
}

} // namespace voxen::gfx
