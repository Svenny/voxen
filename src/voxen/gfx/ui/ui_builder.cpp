#include <voxen/gfx/ui/ui_builder.hpp>

#include <voxen/gfx/gfx_system.hpp>
#include <voxen/util/hash.hpp>

// TODO: cutting through API abstraction
#include <voxen/gfx/vk/vk_device.hpp>
// TODO: not only that, but also using legacy shit
#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/pipeline.hpp>
#include <voxen/client/vulkan/pipeline_layout.hpp>

#include "ui_render_list_builder.hpp"

#include <cassert>
#include <forward_list>

namespace voxen::gfx::ui
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

} // namespace

struct detail::ContainerImpl {
	ContainerImpl *parent = nullptr;
	uint64_t id = 0;

	LayoutSetup layout = {};
	Sizing sizing = {};
	BorderSetup border = {};
	RectangleSetup rectangle = {};

	float x = 0.0f;
	float y = 0.0f;

	float width = 0.0f;
	float height = 0.0f;

	std::vector<ContainerImpl *> children;
};

class detail::UiBuilderImpl {
public:
	UiBuilderImpl() { m_root_container.layout.direction = LayoutDirection::BackToFront; }

	ContainerImpl &pushContainer(DivSetup setup)
	{
		auto &container = m_containers.emplace_front();
		container.parent = std::exchange(m_container_stack_top, &container);
		container.parent->children.emplace_back(&container);
		container.id = calcContainerId(container.parent->id, setup.id, container.parent->children.size());

		container.layout = setup.layout;
		container.sizing = setup.sizing;
		container.border = setup.border;
		container.rectangle = setup.rectangle;

		return container;
	}

	void popContainer(ContainerImpl &container)
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

	void render(int32_t root_width, int32_t root_height, GfxSystem &gfx, VkCommandBuffer cmd_buf)
	{
		m_root_container.x = 0.0f;
		m_root_container.y = 0.0f;

		m_root_container.width = static_cast<float>(root_width);
		m_root_container.height = static_cast<float>(root_height);

		sizeContainer<true>(m_root_container);
		sizeContainer<false>(m_root_container);

		uint32_t root_item_id = m_render_list_builder.addItem(glm::vec2(0, 0), glm::vec2(root_width, root_height));

		std::vector<std::pair<ContainerImpl *, uint32_t>> dfs;
		dfs.emplace_back(&m_root_container, root_item_id);

		while (!dfs.empty()) {
			auto [container, parent_item_id] = dfs.back();
			dfs.pop_back();

			// No need for a new item if there are no children (reuse parent scissor box)
			uint32_t item_id = parent_item_id;
			if (!container->children.empty()) {
				float inner_width = container->width - container->layout.padding.left - container->layout.padding.right;
				float inner_height = container->height - container->layout.padding.top - container->layout.padding.bottom;

				glm::vec2 min(container->x + container->layout.padding.left, container->y + container->layout.padding.top);
				glm::vec2 max = min + glm::vec2(inner_width, inner_height);
				item_id = m_render_list_builder.addItem(min, max);
			}

			if (container->rectangle.color.a != 0) {
				glm::vec2 min(container->x, container->y);
				glm::vec2 max = min + glm::vec2(container->width, container->height);
				m_render_list_builder.addRectangle(min, max, container->rectangle.color, parent_item_id);
			}

			// TODO: cull out-of-screen subtrees?
			for (size_t i = 0; i < container->children.size(); i++) {
				ContainerImpl *child = container->children[i];
				dfs.emplace_back(child, item_id);
			}
		}

		auto draw_cmd = m_render_list_builder.uploadAndClear(*gfx.transientBufferAllocator());

		VkDescriptorBufferInfo vertex_buf_info {
			.buffer = draw_cmd.vertex_buffer_handle,
			.offset = draw_cmd.vertex_buffer_offset,
			.range = draw_cmd.vertex_buffer_byte_range,
		};

		VkDescriptorBufferInfo per_item_buf_info {
			.buffer = draw_cmd.per_item_buffer_handle,
			.offset = draw_cmd.per_item_buffer_offset,
			.range = draw_cmd.per_item_buffer_byte_range,
		};

		VkWriteDescriptorSet descriptors[2] = {
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = VK_NULL_HANDLE,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &vertex_buf_info,
				.pTexelBufferView = nullptr,
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = VK_NULL_HANDLE,
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &per_item_buf_info,
				.pTexelBufferView = nullptr,
			},
		};

		vk::Device &dev = *gfx.device();
		auto &ddt = dev.dt();

		auto &legacy_backend = client::vulkan::Backend::backend();
		auto &pipeline_layout = legacy_backend.pipelineLayoutCollection().uiBasicLayout();

		const glm::vec2 inv_screen_size = glm::vec2(1.0f) / glm::vec2(root_width, root_height);

		ddt.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			legacy_backend.pipelineCollection()[client::vulkan::PipelineCollection::UI_BASIC_PIPELINE]);
		ddt.vkCmdPushConstants(cmd_buf, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec2),
			glm::value_ptr(inv_screen_size));
		ddt.vkCmdPushDescriptorSetKHR(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0,
			std::size(descriptors), descriptors);

		static_assert(std::is_same_v<uint16_t, RenderListBuilder::IndexType>, "UI index type changed");
		ddt.vkCmdBindIndexBuffer(cmd_buf, draw_cmd.index_buffer_handle, draw_cmd.index_buffer_offset,
			VK_INDEX_TYPE_UINT16);

		ddt.vkCmdDrawIndexedIndirect(cmd_buf, draw_cmd.indirect_buffer_handle, draw_cmd.indirect_buffer_offset,
			draw_cmd.draw_count, sizeof(VkDrawIndexedIndirectCommand));
	}

private:
	ContainerImpl m_root_container;
	ContainerImpl *m_container_stack_top = &m_root_container;
	std::forward_list<ContainerImpl> m_containers;

	RenderListBuilder m_render_list_builder;

	template<bool X_AXIS>
	static void sizeContainer(ContainerImpl &container)
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
};

// ScopedContainer

ScopedContainer::~ScopedContainer()
{
	m_ui.popContainer(m_impl);
}

bool ScopedContainer::hovered() const noexcept
{
	// TODO: implement me
	return false;
}

bool ScopedContainer::pressed() const noexcept
{
	// TODO: implement me
	return false;
}

bool ScopedContainer::released() const noexcept
{
	// TODO: implement me
	return false;
}

void ScopedContainer::setColor(PackedColorSrgb color) noexcept
{
	m_impl.rectangle.color = color;
}

// UiBuilder

UiBuilder::UiBuilder() = default;
UiBuilder::~UiBuilder() = default;

auto UiBuilder::div(DivSetup setup) -> ScopedContainer
{
	return ScopedContainer(m_impl.object(), m_impl->pushContainer(setup));
}

void UiBuilder::label(LabelSetup setup)
{
	(void) setup;
}

void UiBuilder::render(int32_t width, int32_t height, GfxSystem &gfx, VkCommandBuffer cmd_buf)
{
	m_impl->render(width, height, gfx, cmd_buf);
}

} // namespace voxen::gfx::ui
