#include <voxen/gfx/ui/ui_system.hpp>

#include <voxen/gfx/ui/ui_builder.hpp>
#include <voxen/util/hash.hpp>

#include "ui_render_list_builder.hpp"
#include "ui_system_impl.hpp"

#include <algorithm>
#include <cassert>

namespace voxen::gfx::ui
{

namespace detail
{

namespace
{

uint64_t calcContainerId(uint64_t parent_id, std::u8string_view id_string, size_t child_index) noexcept
{
	if (id_string.empty()) {
		return Hash::xxh64Fixed(parent_id + child_index);
	}

	return Hash::xxh64Fixed(parent_id + std::hash<std::u8string_view>()(id_string));
}

template<bool X_AXIS>
void sizeContainer(ContainerImpl &container)
{
	const float parent_size = X_AXIS ? container.width : container.height;
	const float parent_padding = X_AXIS
		? container.layout.padding.left + container.layout.padding.right
		: container.layout.padding.top + container.layout.padding.bottom;
	const float parent_child_gap = container.layout.child_gap;
	// Both axes will be non-primary for `BackToFront` layout direction
	const bool on_primary_axis = (X_AXIS && container.layout.direction == LayoutDirection::LeftToRight)
		|| (!X_AXIS && container.layout.direction == LayoutDirection::TopToBottom);

	const float layout_base_coord = X_AXIS
		? container.x + container.layout.padding.left
		: container.y + container.layout.padding.top;
	const Gravity parent_gravity = X_AXIS ? container.layout.x_gravity : container.layout.y_gravity;

	int32_t grow_children_count = 0;
	float inner_content_size = 0.0f;
	float grow_content_size = 0.0f;
	float total_padding = parent_padding;

	std::vector<ContainerImpl *> resizable_children;

	for (size_t i = 0; i < container.children.size(); i++) {
		ContainerImpl *child = container.children[i];
		const AxisSizing child_sizing = X_AXIS ? child->sizing.x : child->sizing.y;
		const float child_size = X_AXIS ? child->width : child->height;

		if (child_sizing.type != AxisSizingType::Percent && child_sizing.type != AxisSizingType::Fixed
			&& child_sizing.min < child_sizing.max) {
			resizable_children.emplace_back(child);
		}

		if (on_primary_axis) {
			inner_content_size += child_sizing.type == AxisSizingType::Percent ? 0.0f : child_size;

			if (child_sizing.type == AxisSizingType::Grow) {
				grow_content_size += child_size;
				grow_children_count++;
			}

			if (i > 0) {
				inner_content_size += parent_child_gap;
				total_padding += parent_child_gap;
			}
		} else if (child_sizing.type != AxisSizingType::Percent) {
			inner_content_size = std::max(inner_content_size, child_size);
		}
	}

	// Size percentage children
	for (ContainerImpl *child : container.children) {
		const AxisSizing child_sizing = X_AXIS ? child->sizing.x : child->sizing.y;
		float &child_size = X_AXIS ? child->width : child->height;

		if (child_sizing.type == AxisSizingType::Percent) {
			child_size = (parent_size - total_padding) * child_sizing.max;

			if (on_primary_axis) {
				inner_content_size += child_size;
			} else {
				inner_content_size = std::max(inner_content_size, child_size);
			}
		}
	}

	if (on_primary_axis) {
		// Ignore small roundoff errors
		constexpr float TOLERANCE = 0.01f;

		const float distribute_size = parent_size - parent_padding - inner_content_size;

		if (distribute_size < -TOLERANCE) {
			// Not enough room for the content, compress children
			// TODO: compress children
		} else if (distribute_size > TOLERANCE && grow_children_count > 0) {
			// Have extra room, allow growing containers to expand
			float target_grown_size = (distribute_size + grow_content_size) / static_cast<float>(grow_children_count);

			for (ContainerImpl *child : container.children) {
				const AxisSizing child_sizing = X_AXIS ? child->sizing.x : child->sizing.y;
				float &child_size = X_AXIS ? child->width : child->height;

				if (child_sizing.type == AxisSizingType::Grow) {
					// TODO: what if `target_grown_size` is less than minimal child size?
					child_size = target_grown_size;
				}
			}
		}

		// All children sizes along the primary axis are known, now position them
		float current_coord = layout_base_coord;

		if (parent_gravity != Gravity::Min) {
			// Add 0.5 or 1 of remaining undistributed inner size to enforce center/max gravity
			float undistributed_size = parent_size - parent_padding;

			for (ContainerImpl *child : container.children) {
				undistributed_size -= X_AXIS ? child->width : child->height;
			}

			if (!container.children.empty()) {
				undistributed_size -= parent_child_gap * static_cast<float>(container.children.size() - 1);
			}

			undistributed_size = std::max(0.0f, undistributed_size);

			if (parent_gravity == Gravity::Center) {
				current_coord += 0.5f * undistributed_size;
			} else {
				current_coord += undistributed_size;
			}
		}

		for (ContainerImpl *child : container.children) {
			const float child_size = X_AXIS ? child->width : child->height;
			float &child_coord = X_AXIS ? child->x : child->y;

			child_coord = current_coord;
			current_coord += child_size + parent_child_gap;
		}
	} else { // not on primary axis
		const float parent_inner_size = std::max(0.0f, parent_size - parent_padding);

		for (ContainerImpl *child : container.children) {
			const AxisSizing child_sizing = X_AXIS ? child->sizing.x : child->sizing.y;
			float &child_size = X_AXIS ? child->width : child->height;
			float &child_coord = X_AXIS ? child->x : child->y;

			if (child_sizing.type == AxisSizingType::Fit) {
				child_size = std::clamp(child_size, child_sizing.min, parent_inner_size);
			} else if (child_sizing.type == AxisSizingType::Grow) {
				child_size = std::min(parent_inner_size, child_sizing.max);
			}

			// Position child along this axis
			const float free_inner_size = std::max(0.0f, parent_inner_size - child_size);

			if (parent_gravity == Gravity::Min) {
				child_coord = layout_base_coord;
			} else if (parent_gravity == Gravity::Center) {
				child_coord = layout_base_coord + 0.5f * free_inner_size;
			} else {
				child_coord = layout_base_coord + free_inner_size;
			}
		}
	}

	// Process children recursively.
	// TODO: recursion can get quite deep for complex layout, maybe better use stack/queue (DFS/BFS)
	for (ContainerImpl *child : container.children) {
		sizeContainer<X_AXIS>(*child);
	}
}

} // namespace

ContainerImpl &UiSystemImpl::pushContainer(DivSetup setup)
{
	auto &container = *m_scratch->make<ContainerImpl>(*m_scratch);
	container.parent = std::exchange(m_container_stack_top, &container);
	container.parent->children.emplace_back(&container);
	container.id = calcContainerId(container.parent->id, setup.id, container.parent->children.size());

	container.layout = setup.layout;
	container.sizing = setup.sizing;
	container.border = setup.border;
	container.rectangle = setup.rectangle;

	// If parent is hovered then this container might as well be.
	// Find it in the previous frame ghosts and check.
	if (container.parent->hovered) {
		ContainerGhost dummy_ghost {};
		dummy_ghost.id = container.id;

		auto iter = std::lower_bound(m_prev_frame_containers.begin(), m_prev_frame_containers.end(), dummy_ghost);
		if (iter != m_prev_frame_containers.end() && iter->id == container.id) {
			// Found this container in the previous frame.
			// Check its calculated layout against the current mouse position.
			glm::vec2 cur_mouse = m_this_frame_data.cursor_position;
			glm::vec2 pos_min { iter->x, iter->y };
			glm::vec2 pos_max { iter->x + iter->width, iter->y + iter->height };
			if (cur_mouse.x >= pos_min.x && cur_mouse.y >= pos_min.y && cur_mouse.x <= pos_max.x
				&& cur_mouse.y <= pos_max.y) {
				// TODO: we ignore stack layouts, multiple items can be hovered at once.
				// The correct way is to query cursor (X, Y) in previous frame layout
				// and check if this item was covered by something else.
				container.hovered = true;
				container.pressed = m_this_frame_data.left_button_pressed;
				container.pressed_changed_this_frame = (container.pressed != iter->pressed);
			}
		}
	}

	return container;
}

void UiSystemImpl::popContainer(ContainerImpl &container)
{
	// Add child element sizes
	const float width_padding = container.layout.padding.left + container.layout.padding.right;
	const float height_padding = container.layout.padding.top + container.layout.padding.bottom;

	container.width = width_padding;
	container.height = height_padding;

	if (container.layout.direction == LayoutDirection::LeftToRight) {
		// Horizontal layout - add in X, max in Y
		for (ContainerImpl *child : container.children) {
			container.width += child->width;
			container.height = std::max(container.height, child->height + height_padding);
		}

		if (!container.children.empty()) {
			container.width += static_cast<float>(container.children.size() - 1) * container.layout.child_gap;
		}
	} else {
		// Vertical layout - add in Y, max in X
		for (ContainerImpl *child : container.children) {
			container.width = std::max(container.width, child->width + width_padding);
			container.height += child->height;
		}

		if (!container.children.empty()) {
			container.height += static_cast<float>(container.children.size() - 1) * container.layout.child_gap;
		}
	}

	if (container.sizing.x.type != AxisSizingType::Percent) {
		// Clamp element width to user-provided min/max
		container.width = std::clamp(container.width, container.sizing.x.min, container.sizing.x.max);
	} else {
		container.width = 0.0f;
	}

	if (container.sizing.y.type != AxisSizingType::Percent) {
		// Clamp element height to user-provided min/max
		container.height = std::clamp(container.height, container.sizing.y.min, container.sizing.y.max);
	} else {
		container.height = 0.0f;
	}

	// Update container stack
	assert(&container == m_container_stack_top);
	m_container_stack_top = container.parent;
}

void UiSystemImpl::beginFrame(ScratchMemoryAllocatorScope &scratch, UiSystem::PerFrameData per_frame)
{
	m_scratch = &scratch;

	m_prev_frame_data = m_this_frame_data;
	m_this_frame_data = per_frame;

	// Create and set up the root container
	m_root_container.reset(scratch.make<ContainerImpl>(*m_scratch));
	m_container_stack_top = m_root_container.get();

	// Don't do any hovered checks at all if cursor is outside the window.
	// This will recursively disable hovered check for child containers.
	m_root_container->hovered = false;

	if (per_frame.cursor_position.x >= 0.0 && per_frame.cursor_position.y >= 0.0
		&& per_frame.cursor_position.x <= per_frame.window_resolution.width
		&& per_frame.cursor_position.y <= per_frame.window_resolution.height) {
		m_root_container->hovered = true;
	}

	m_root_container->layout.direction = LayoutDirection::BackToFront;

	m_root_container->x = 0.0f;
	m_root_container->y = 0.0f;
	m_root_container->width = static_cast<float>(per_frame.window_resolution.width);
	m_root_container->height = static_cast<float>(per_frame.window_resolution.height);
}

RenderData UiSystemImpl::endFrame()
{
	sizeContainer<true>(*m_root_container);
	sizeContainer<false>(*m_root_container);

	// Prime render list builder with the number of rectangles added in previous frame.
	// Should be close to perfect estimate when UI layout does not change between frames.
	RenderListBuilder render_list_builder(*m_scratch, m_prev_frame_render_data_rectangles);
	m_prev_frame_render_data_rectangles = 0;

	uint32_t root_item_id = render_list_builder.addItem(glm::vec2(0, 0),
		glm::vec2(m_root_container->width, m_root_container->height));

	scratch_vector<std::pair<ContainerImpl *, uint32_t>> dfs(*m_scratch);
	// Estimate the maximal container stack depth
	dfs.reserve(16);
	dfs.emplace_back(m_root_container.get(), root_item_id);

	// Create new ghosts of containers from this frame while iterating
	m_prev_frame_containers.clear();

	while (!dfs.empty()) {
		auto [container, parent_item_id] = dfs.back();
		dfs.pop_back();

		m_prev_frame_containers.emplace_back(ContainerGhost {
			.id = container->id,
			.x = container->x,
			.y = container->y,
			.width = container->width,
			.height = container->height,
			.pressed = container->pressed,
		});

		// No need for a new item if there are no children (reuse parent scissor box)
		uint32_t item_id = parent_item_id;
		if (!container->children.empty()) {
			float inner_width = container->width - container->layout.padding.left - container->layout.padding.right;
			float inner_height = container->height - container->layout.padding.top - container->layout.padding.bottom;

			glm::vec2 min(container->x + container->layout.padding.left, container->y + container->layout.padding.top);
			glm::vec2 max = min + glm::vec2(inner_width, inner_height);
			item_id = render_list_builder.addItem(min, max);
		}

		if (container->rectangle.color.a != 0) {
			glm::vec2 min(container->x, container->y);
			glm::vec2 max = min + glm::vec2(container->width, container->height);
			render_list_builder.addRectangle(min, max, container->rectangle.color, parent_item_id);
			m_prev_frame_render_data_rectangles++;
		}

		// TODO: cull out-of-screen subtrees?
		for (size_t i = 0; i < container->children.size(); i++) {
			ContainerImpl *child = container->children[i];
			dfs.emplace_back(child, item_id);
		}
	}

	// Sort ghosts to allow for fast binary search in the next frame
	std::sort(m_prev_frame_containers.begin(), m_prev_frame_containers.end());

	// Reset this frame containers, we no longer need them.
	// XXX: everything in containers is scratch-allocated, this does nothing.
	m_root_container.reset();
	m_container_stack_top = nullptr;

	// `render_list_builder` will destroy after this line
	// so returned struct will have kinda dangling pointers.
	// But they all point to scratch memory so are safe to use
	// until `m_scratch` scope closes (somewhere outside).
	return render_list_builder.produceRenderData();
}

} // namespace detail

UiSystem::UiSystem() = default;

UiSystem::~UiSystem() = default;

UiBuilder UiSystem::beginFrame(ScratchMemoryAllocatorScope &scratch, PerFrameData per_frame)
{
	auto &impl = m_impl.object();
	impl.beginFrame(scratch, per_frame);
	return UiBuilder(impl);
}

RenderData UiSystem::endFrame()
{
	return m_impl->endFrame();
}

} // namespace voxen::gfx::ui
