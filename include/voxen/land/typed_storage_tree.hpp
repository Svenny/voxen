#pragma once

#include <voxen/land/land_storage_tree.hpp>

#include <extras/attributes.hpp>

namespace voxen::land
{

namespace detail
{

template<typename Shared, typename Private>
struct TypedStorageItem {
public:
	struct Dummy {};

	[[EXTRAS_NO_UNIQUE_ADDRESS]] std::conditional_t<std::is_same_v<Shared, void>, Dummy, Shared> m_shared;
	[[EXTRAS_NO_UNIQUE_ADDRESS]] std::conditional_t<std::is_same_v<Private, void>, Dummy, Private> m_private;
};

} // namespace detail

template<typename ChunkShared, typename ChunkPrivate, typename DuoctreeShared, typename DuoctreePrivate>
class TypedStorageTree {
public:
	constexpr static bool HAS_CHUNK_SHARED = !std::is_same_v<ChunkShared, void>;
	constexpr static bool HAS_CHUNK_PRIVATE = !std::is_same_v<ChunkPrivate, void>;
	constexpr static bool HAS_CHUNK_STORAGE = HAS_CHUNK_SHARED || HAS_CHUNK_PRIVATE;
	constexpr static bool HAS_DUOCTREE_SHARED = !std::is_same_v<DuoctreeShared, void>;
	constexpr static bool HAS_DUOCTREE_PRIVATE = !std::is_same_v<DuoctreePrivate, void>;
	constexpr static bool HAS_DUOCTREE_STORAGE = HAS_DUOCTREE_SHARED || HAS_DUOCTREE_PRIVATE;

	TypedStorageTree() noexcept : m_tree(makeCtl()) {}
	TypedStorageTree(TypedStorageTree &&other) noexcept = default;
	TypedStorageTree(const TypedStorageTree &other) noexcept = default;
	TypedStorageTree &operator=(TypedStorageTree &&other) noexcept = default;
	TypedStorageTree &operator=(const TypedStorageTree &other) noexcept = default;
	~TypedStorageTree() = default;

	template<typename TChunkShared, typename TChunkPrivate, typename TDuoctreeShared, typename TDuoctreePrivate,
		typename TCopier>
	void copyFrom(const TypedStorageTree<TChunkShared, TChunkPrivate, TDuoctreeShared, TDuoctreePrivate> &other,
		TCopier &&copier)
	{
		using TOther = std::remove_cvref_t<decltype(other)>;
		using TTCopier = std::remove_cvref_t<TCopier>;

		StorageTree::UserDataCopyFn user_data_copy_fn =
			[](void *ctx, ChunkKey key, WorldTickId old_version, WorldTickId new_version, void *copy_to,
				const void *copy_from) {
				TTCopier &t_copier = *reinterpret_cast<TTCopier *>(ctx);

				if constexpr (HAS_CHUNK_STORAGE && TOther::HAS_CHUNK_SHARED) {
					const auto &from = *TOther::chunkSharedAccess(copy_from);

					if constexpr (HAS_CHUNK_SHARED && HAS_CHUNK_PRIVATE) {
						t_copier(key, old_version, new_version, *chunkSharedAccess(copy_to), *chunkPrivateAccess(copy_to),
							from);
					} else if constexpr (HAS_CHUNK_SHARED) {
						t_copier(key, old_version, new_version, *chunkSharedAccess(copy_to), from);
					} else if constexpr (HAS_CHUNK_PRIVATE) {
						t_copier(key, old_version, new_version, *chunkPrivateAccess(copy_to), from);
					}
				}

				if constexpr (HAS_DUOCTREE_STORAGE && TOther::HAS_DUOCTREE_SHARED) {
					const auto &from = *TOther::duoctreeSharedAccess(copy_from);

					if constexpr (HAS_DUOCTREE_SHARED && HAS_DUOCTREE_PRIVATE) {
						t_copier(key, old_version, new_version, *duoctreeSharedAccess(copy_to),
							*duoctreePrivateAccess(copy_to), from);
					} else if constexpr (HAS_DUOCTREE_SHARED) {
						t_copier(key, old_version, new_version, *duoctreeSharedAccess(copy_to), from);
					} else if constexpr (HAS_DUOCTREE_PRIVATE) {
						t_copier(key, old_version, new_version, *duoctreePrivateAccess(copy_to), from);
					}
				}
			};

		m_tree.copyFrom(other.m_tree, user_data_copy_fn, std::addressof(copier));
	}

private:
	StorageTree m_tree;

	static ChunkShared *chunkSharedAccess(void *place) noexcept
	{
		return std::launder(reinterpret_cast<ChunkShared *>(place));
	}

	static const ChunkShared *chunkSharedAccess(const void *place) noexcept
	{
		return std::launder(reinterpret_cast<const ChunkShared *>(place));
	}

	static DuoctreeShared *duoctreeSharedAccess(void *place) noexcept
	{
		return std::launder(reinterpret_cast<DuoctreeShared *>(place));
	}

	static const DuoctreeShared *duoctreeSharedAccess(const void *place) noexcept
	{
		return std::launder(reinterpret_cast<const DuoctreeShared *>(place));
	}

	static ChunkPrivate *chunkPrivateAccess(void *place) noexcept
	{
		using TSI = detail::TypedStorageItem<ChunkShared, ChunkPrivate>;
		return &std::launder(reinterpret_cast<TSI *>(place))->m_private;
	}

	static DuoctreePrivate *duoctreePrivateAccess(void *place) noexcept
	{
		using TSI = detail::TypedStorageItem<DuoctreeShared, DuoctreePrivate>;
		return &std::launder(reinterpret_cast<TSI *>(place))->m_private;
	}

	static StorageTreeControl makeCtl() noexcept
	{
		using CTSI = detail::TypedStorageItem<ChunkShared, ChunkPrivate>;
		using DTSI = detail::TypedStorageItem<DuoctreeShared, DuoctreePrivate>;

		StorageTreeControl ctl;
		ctl.chunk_user_data_size = HAS_CHUNK_STORAGE ? sizeof(CTSI) : 0;
		ctl.duoctree_user_data_size = HAS_DUOCTREE_STORAGE ? sizeof(DTSI) : 0;

		ctl.chunk_user_data_default_ctor = [](void * /*ctx*/, ChunkKey /*key*/, void *place) {
			if constexpr (HAS_CHUNK_SHARED) {
				new (chunkSharedAccess(place)) ChunkShared();
			}
			if constexpr (HAS_CHUNK_PRIVATE) {
				new (chunkPrivateAccess(place)) ChunkPrivate();
			}
		};

		ctl.chunk_user_data_copy_ctor = [](void * /*ctx*/, ChunkKey /*key*/, void *place, void *copy_from) {
			if constexpr (HAS_CHUNK_SHARED) {
				new (chunkSharedAccess(place)) ChunkShared(*chunkSharedAccess(copy_from));
			}
			if constexpr (HAS_CHUNK_PRIVATE) {
				// Move private part ownership
				new (chunkPrivateAccess(place)) ChunkPrivate(std::move(*chunkPrivateAccess(copy_from)));
			}
		};

		ctl.chunk_user_data_dtor = [](void * /*ctx*/, ChunkKey /*key*/, void *place) noexcept {
			if constexpr (HAS_CHUNK_SHARED) {
				chunkSharedAccess(place)->~ChunkShared();
			}
			if constexpr (HAS_CHUNK_PRIVATE) {
				chunkPrivateAccess(place)->~ChunkPrivate();
			}
		};

		ctl.duoctree_user_data_default_ctor = [](void * /*ctx*/, ChunkKey /*key*/, void *place) {
			if constexpr (HAS_DUOCTREE_SHARED) {
				new (duoctreeSharedAccess(place)) DuoctreeShared();
			}
			if constexpr (HAS_DUOCTREE_PRIVATE) {
				new (duoctreePrivateAccess(place)) DuoctreePrivate();
			}
		};

		ctl.duoctree_user_data_copy_ctor = [](void * /*ctx*/, ChunkKey /*key*/, void *place, void *copy_from) {
			if constexpr (HAS_DUOCTREE_SHARED) {
				new (duoctreeSharedAccess(place)) DuoctreeShared(*duoctreeSharedAccess(copy_from));
			}
			if constexpr (HAS_DUOCTREE_PRIVATE) {
				// Move private part ownership
				new (duoctreePrivateAccess(place)) DuoctreePrivate(std::move(*duoctreePrivateAccess(copy_from)));
			}
		};

		ctl.duoctree_user_data_dtor = [](void * /*ctx*/, ChunkKey /*key*/, void *place) noexcept {
			if constexpr (HAS_DUOCTREE_SHARED) {
				duoctreeSharedAccess(place)->~DuoctreeShared();
			}
			if constexpr (HAS_DUOCTREE_PRIVATE) {
				duoctreePrivateAccess(place)->~DuoctreePrivate();
			}
		};

		return ctl;
	}

	template<typename, typename, typename, typename>
	friend class TypedStorageTree;
};

} // namespace voxen::land
