#include <voxen/gfx/gfx_land_loader.hpp>

#include <voxen/common/world_state.hpp>
#include <voxen/gfx/gfx_system.hpp>
#include <voxen/gfx/vk/vk_mesh_streamer.hpp>
#include <voxen/land/land_messages.hpp>
#include <voxen/land/land_service.hpp>
#include <voxen/land/land_utils.hpp>
#include <voxen/svc/messaging_service.hpp>
#include <voxen/svc/service_locator.hpp>

#include <optional>

namespace voxen::gfx
{

namespace
{

constexpr UID LAND_LOADER_DOMAIN_UID("0a7ac6a1-18693cb0-21c2e1bb-912579d5");

constexpr uint32_t LAST_RENDERED_LOD = land::Consts::NUM_LOD_SCALES - 1;
constexpr uint32_t LOAD_BOX_DISTANCE_LOD0 = 8;
constexpr uint32_t LOAD_BOX_DISTANCE_LODN = 5;

} // namespace

class detail::LandLoaderImpl {
public:
	using DrawCommand = LandLoader::DrawCommand;
	using DrawList = LandLoader::DrawList;

	LandLoaderImpl(GfxSystem &gfx, svc::ServiceLocator &svc) : m_gfx(gfx)
	{
		// HELL YEAH!!!
		UID my_uid = UID::generateRandom();
		m_message_sender = svc.requestService<svc::MessagingService>().createSender(my_uid);
	}

	void onNewState(const WorldState &state)
	{
		const auto &new_land = state.landState();
		last_known_pseudo_chunk_surface_table = new_land.pseudo_chunk_surface_table;
	}

	// Request streaming the new/updated mesh for chunk at `key`.
	// If some mesh (not necessarily the newest one) is available for rendering
	// in this frame, returns draw command for it, otherwise returns nullopt.
	std::optional<DrawCommand> makeDrawCommand(land::ChunkKey key)
	{
		auto *table_item = last_known_pseudo_chunk_surface_table.find(key);

		if (!table_item) {
			// Unknown chunk
			return std::nullopt;
		}

		if (!table_item->hasValue() || table_item->value().empty()) {
			// Known empty chunk
			DrawCommand dcmd {};
			dcmd.chunk_key = key;
			return dcmd;
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
			return std::nullopt;
		}

		// Have some data available
		DrawCommand dcmd {};
		dcmd.chunk_key = key;

		dcmd.index_buffer = mesh_info.substreams[2].vk_buffer;
		dcmd.first_index = mesh_info.substreams[2].first_element;
		dcmd.num_indices = mesh_info.substreams[2].num_elements;

		dcmd.pos_data_address = mesh_info.substreams[0].buffer_gpu_address
			+ mesh_info.substreams[0].first_element * sizeof(land::PseudoSurfaceVertexPosition);
		dcmd.attrib_data_address = mesh_info.substreams[1].buffer_gpu_address
			+ mesh_info.substreams[1].first_element * sizeof(land::PseudoSurfaceVertexAttributes);

		return dcmd;
	}

	// Fills draw list for LOD subtree at `chunk_base` and `level`.
	// Returns true if all subtree volume was successfully covered with
	// draw commands. If false is returned, there will be non-rendered holes
	// in places of some chunks (unless you cover the area with lower res).
	bool makeDrawListSubtree(DrawList &dlist, const glm::ivec3 chunk_base, const uint8_t level)
	{
		const auto &lod_box = m_chunk_ticket_boxes[level];
		if (chunk_base.x < lod_box.begin.x || chunk_base.x >= lod_box.end.x || chunk_base.y < lod_box.begin.y
			|| chunk_base.y >= lod_box.end.y || chunk_base.z < lod_box.begin.z || chunk_base.z >= lod_box.end.z) {
			// Out of render area for this LOD
			return false;
		}

		// Recursively call this function for 8 child subtrees. Don't call when `level` is 0.
		auto try_finer_level = [&]() {
			const uint8_t n = level - 1;
			const int32_t k = 1 << n;

			bool res = makeDrawListSubtree(dlist, chunk_base, n);
			res &= makeDrawListSubtree(dlist, chunk_base + glm::ivec3(k, 0, 0), n);
			res &= makeDrawListSubtree(dlist, chunk_base + glm::ivec3(0, k, 0), n);
			res &= makeDrawListSubtree(dlist, chunk_base + glm::ivec3(0, 0, k), n);
			res &= makeDrawListSubtree(dlist, chunk_base + glm::ivec3(k, k, 0), n);
			res &= makeDrawListSubtree(dlist, chunk_base + glm::ivec3(0, k, k), n);
			res &= makeDrawListSubtree(dlist, chunk_base + glm::ivec3(k, 0, k), n);
			res &= makeDrawListSubtree(dlist, chunk_base + glm::ivec3(k, k, k), n);

			return res;
		};

		land::ChunkKey key(chunk_base, level);

		// Make draw command before trying finer resolution - this will force streaming
		// lower LOD to have it immediately available even if it won't be rendered. We
		// will have a few number of low-resolution chunks always "preempted" by high-resolution
		// ones which is a bit of VRAM waste but we get the ability to switch LODs immediately.
		// Also, some of these low-resolution chunks can be used for long-distance shadows.
		std::optional<DrawCommand> maybe_dcmd = makeDrawCommand(key);

		// Some of the finer ones might be missing but we can substitute it with the current level.
		// Remember draw list position and prepare to unwind it later.
		auto rewind_position = ptrdiff_t(dlist.size());

		// Try finer LODs. Might seem like overkill but out-of-area
		// check above should eliminate most of the excessive work.
		if (level > 0 && try_finer_level()) {
			return true;
		}

		if (!maybe_dcmd.has_value()) {
			// We can't fully substitute a missing finer level.
			// If a certain chunk is missing all the way down to LOD0, there will be a
			// non-rendered hole in its place (not enough information to draw anything).
			//
			// Should occur only during the initial loading and in some brief transients
			// like teleporting to an entirely new location. Otherwise that forced low-res
			// streaming stuff above should cover us and provide at least something to draw.
			return false;
		}

		if (maybe_dcmd->num_indices == 0) {
			// Known empty chunk.
			// XXX: shouldn't we unwind finer level commands here too?
			return true;
		}

		// Finer levels will have Z-fighting or other issues from overlap
		// with the coarser one, so remove (unwind) potentially added commands.
		dlist.erase(dlist.begin() + rewind_position, dlist.end());
		dlist.emplace_back(*maybe_dcmd);
		return true;
	}

	void makeDrawList(const glm::dvec3 &viewpoint, DrawList &dlist)
	{
		// Request land service to generate surface for chunks in render area.
		// If N = CHUNK_SIZE_METRES
		// ...
		// [-N;0) - chunk -1
		// [0;N) - chunk 0
		// [N;2N) - chunk 1
		// ...
		// So to get chunk coordinate, divide by N and round down.

		for (uint32_t lod = 0; lod <= LAST_RENDERED_LOD; lod++) {
			// Render area size in chunks
			double load_box_chunks = double(lod ? LOAD_BOX_DISTANCE_LODN << lod : LOAD_BOX_DISTANCE_LOD0);
			glm::dvec3 box_lo = viewpoint / land::Consts::CHUNK_SIZE_METRES - load_box_chunks;
			glm::dvec3 box_hi = viewpoint / land::Consts::CHUNK_SIZE_METRES + load_box_chunks;

			// Load box coords in chunks
			glm::ivec3 lo_coord = glm::ivec3(glm::floor(box_lo));
			glm::ivec3 hi_coord = glm::ivec3(glm::ceil(box_hi));

			// Align to two times the scaled chunk boundaries, basically to the parent LOD grid.
			// This gives a nice property: for every chunk key with LOD > 0 either this key
			// or all of its 8 children are in the renderable set. So we can simplify mesh
			// selection logic: if surfaces for all children are available, take those and stop.
			//
			// Strictly speaking, we could do that without this alignment, but then
			// more children surfaces will be needlessly loaded only to be not enough
			// to fill up the whole 2x2x2 "child volume".
			// XXX: explain this better, I can't pick a good wording. Draw it maybe?
			//
			// Masking works equally well for negative numbers.
			const int32_t round_add = (1 << (lod + 1)) - 1;
			const int32_t round_mask = ~round_add;

			// Align lower end down
			lo_coord = lo_coord & round_mask;
			// Align upper end up
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

					// Reset the handle to not leak pipe memory.
					// XXX: this interface is not nice, makes it REALLY easy to leak.
					m_chunk_ticket_requests[lod].reset();
				}
			} else {
				m_chunk_ticket_boxes[lod] = wanted_box;
				m_chunk_ticket_requests[lod]
					= m_message_sender.requestWithHandle<land::ChunkTicketRequestMessage>(land::LandService::SERVICE_UID,
						wanted_box);
			}
		}

		dlist.clear();

		// Iterate over every chunk in the lowest-resolution (largest) LOD box.
		// Collect draws for the whole subtree - the function will recurse as needed.
		const land::ChunkKey lo = m_chunk_ticket_boxes[LAST_RENDERED_LOD].begin;
		const land::ChunkKey hi = m_chunk_ticket_boxes[LAST_RENDERED_LOD].end;
		const int32_t step = lo.scaleMultiplier();

		for (int64_t y = lo.y; y < hi.y; y += step) {
			for (int64_t x = lo.x; x < hi.x; x += step) {
				for (int64_t z = lo.z; z < hi.z; z += step) {
					makeDrawListSubtree(dlist, glm::ivec3(x, y, z), lo.scale_log2);
				}
			}
		}
	}

	GfxSystem &m_gfx;
	svc::MessageSender m_message_sender;

	land::LandState::PseudoChunkSurfaceTable last_known_pseudo_chunk_surface_table;

	land::ChunkTicketBoxArea m_chunk_ticket_boxes[land::Consts::NUM_LOD_SCALES];
	land::ChunkTicket m_chunk_tickets[land::Consts::NUM_LOD_SCALES];
	svc::RequestHandle<land::ChunkTicketRequestMessage> m_chunk_ticket_requests[land::Consts::NUM_LOD_SCALES];
};

LandLoader::LandLoader(GfxSystem &gfx, svc::ServiceLocator &svc) : m_impl(gfx, svc) {}

LandLoader::~LandLoader() = default;

void LandLoader::onNewState(const WorldState &state)
{
	m_impl->onNewState(state);
}

void LandLoader::makeDrawList(const glm::dvec3 &viewpoint, DrawList &dlist)
{
	m_impl->makeDrawList(viewpoint, dlist);
}

} // namespace voxen::gfx
