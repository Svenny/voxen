#include <voxen/gfx/ui/ui_builder.hpp>

#include <voxen/util/hash.hpp>

#include <cassert>

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
	BorderSetup border = {};
	RectangleSetup rectangle = {};

	float width = 0.0f;
	float height = 0.0f;

	std::vector<ContainerImpl *> children;
};

class detail::UiBuilderImpl {
public:
	ContainerImpl &pushContainer(DivSetup setup)
	{
		auto &container = m_containers.emplace_back();
		container.parent = std::exchange(m_container_stack_top, &container);
		container.parent->children.emplace_back(&container);
		container.id = calcContainerId(container.parent->id, setup.id, container.parent->children.size());

		container.layout = setup.layout;
		container.border = setup.border;
		container.rectangle = setup.rectangle;

		return container;
	}

	void popContainer(ContainerImpl &container)
	{
		// Add child element sizes
		const float width_padding = container.layout.padding.left + container.layout.padding.right;
		const float height_padding = container.layout.padding.top + container.layout.padding.bottom;

		if (container.layout.direction == LayoutDirection::LeftToRight) {
			// Horizontal layout - add in X, max in Y
			container.width = width_padding;
			container.height = 0.0f;

			for (ContainerImpl *child : container.children) {
				container.width += child->width;
				container.height = std::max(container.height, child->height + height_padding);
			}

			if (!container.children.empty()) {
				container.width += static_cast<float>(container.children.size() - 1) * container.layout.child_gap;
			}
		} else {
			// Vertical layout - add in Y, max in X
			container.width = 0.0f;
			container.height = height_padding;

			for (ContainerImpl *child : container.children) {
				container.width = std::max(container.width, child->width + width_padding);
				container.height += child->height;
			}

			if (!container.children.empty()) {
				container.height += static_cast<float>(container.children.size() - 1) * container.layout.child_gap;
			}
		}

		if (container.layout.x_sizing.type != LayoutSizingType::Percent) {
			// Clamp element width to user-provided min/max
			container.width = std::clamp(container.width, container.layout.x_sizing.min, container.layout.x_sizing.max);
		} else {
			container.width = 0.0f;
		}

		if (container.layout.y_sizing.type != LayoutSizingType::Percent) {
			// Clamp element height to user-provided min/max
			container.height = std::clamp(container.height, container.layout.y_sizing.min, container.layout.y_sizing.max);
		} else {
			container.height = 0.0f;
		}

		// Update container stack
		assert(&container == m_container_stack_top);
		m_container_stack_top = container.parent;
	}

	void computeLayout(int32_t root_width, int32_t root_height)
	{
		m_root_container.width = static_cast<float>(root_width);
		m_root_container.height = static_cast<float>(root_height);

		sizeContainer<true>(m_root_container);
		sizeContainer<false>(m_root_container);
	}

private:
	ContainerImpl m_root_container;

	std::list<ContainerImpl> m_containers;

	ContainerImpl *m_container_stack_top = &m_root_container;

	template<bool X_AXIS>
	static void sizeContainer(ContainerImpl &container)
	{
		const float parent_size = X_AXIS ? container.width : container.height;
		const float parent_padding = X_AXIS
			? container.layout.padding.left + container.layout.padding.right
			: container.layout.padding.top + container.layout.padding.bottom;
		const float parent_child_gap = container.layout.child_gap;
		const bool on_primary_axis = (X_AXIS && container.layout.direction == LayoutDirection::LeftToRight)
			|| (!X_AXIS && container.layout.direction == LayoutDirection::TopToBottom);

		int32_t grow_children_count = 0;
		float inner_content_size = 0.0f;
		float grow_content_size = 0.0f;
		float total_padding = parent_padding;

		std::vector<ContainerImpl *> resizable_children;

		for (size_t i = 0; i < container.children.size(); i++) {
			ContainerImpl *child = container.children[i];
			const LayoutSizing child_sizing = X_AXIS ? child->layout.x_sizing : child->layout.y_sizing;
			const float child_size = X_AXIS ? child->width : child->height;

			if (child_sizing.type != LayoutSizingType::Percent && child_sizing.min < child_sizing.max) {
				resizable_children.emplace_back(child);
			}

			if (on_primary_axis) {
				inner_content_size += child_sizing.type == LayoutSizingType::Percent ? 0.0f : child_size;

				if (child_sizing.type == LayoutSizingType::Grow) {
					grow_content_size += child_size;
					grow_children_count++;
				}

				if (i > 0) {
					inner_content_size += parent_child_gap;
					total_padding += parent_child_gap;
				}
			} else {
				inner_content_size = std::max(inner_content_size, child_size);
			}
		}

		// Size percentage children
		for (ContainerImpl *child : container.children) {
			const LayoutSizing child_sizing = X_AXIS ? child->layout.x_sizing : child->layout.y_sizing;
			float &child_size = X_AXIS ? child->width : child->height;

			if (child_sizing.type == LayoutSizingType::Percent) {
				child_size = (parent_size - total_padding) * child_sizing.max;

				if (on_primary_axis) {
					inner_content_size += child_size;
				}
			}
		}

		if (on_primary_axis) {
			// Ignore small roundoff errors
			constexpr float TOLERANCE = 0.01f;

			const float distribute_size = parent_size - parent_padding - inner_content_size;

			if (distribute_size < -TOLERANCE) {
				// Not enough room for the content, compress children
			} else if (distribute_size > TOLERANCE && grow_children_count > 0) {
				// Have extra room, allow growing containers to expand
				float target_grown_size = (distribute_size + grow_content_size) / static_cast<float>(grow_children_count);

				for (ContainerImpl *child : container.children) {
					const LayoutSizing child_sizing = X_AXIS ? child->layout.x_sizing : child->layout.y_sizing;
					float &child_size = X_AXIS ? child->width : child->height;

					if (child_sizing.type == LayoutSizingType::Grow) {
						// TODO: what if `target_grown_size` is less than minimal child size?
						child_size = target_grown_size;
					}
				}
			}

		} else { // !on_primary_axis
			for (ContainerImpl *child : container.children) {
				const LayoutSizing child_sizing = X_AXIS ? child->layout.x_sizing : child->layout.y_sizing;
				float &child_size = X_AXIS ? child->width : child->height;

				float max_size = parent_size - parent_padding;
				if (child_sizing.type == LayoutSizingType::Fit) {
					child_size = std::clamp(child_size, child_sizing.min, max_size);
				} else if (child_sizing.type == LayoutSizingType::Grow) {
					child_size = std::min(max_size, child_sizing.max);
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

void UiBuilder::computeLayout(int32_t root_width, int32_t root_height)
{
	m_impl->computeLayout(root_width, root_height);
}

} // namespace voxen::gfx::ui
