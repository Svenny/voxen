#include <voxen/common/v8g_flat_map.hpp>
#include <voxen/common/v8g_flat_map_impl.hpp>

#include <catch2/catch.hpp>

namespace voxen
{

TEST_CASE("'V8gFlatMap' sanity check", "[voxen::v8g_flat_map]")
{
	struct SimpleItem {
		int value;
	};

	using VFM = V8gFlatMap<uint32_t, SimpleItem>;
	using IVFM = V8gFlatMap<uint32_t, SimpleItem, V8gStoragePolicy::Immutable>;

	VFM vfm;

	uint64_t timeline = 5;
	vfm.insert(timeline, 5, { .value = 1 });
	vfm.insert(timeline, 10, { .value = 2 });
	vfm.insert(timeline, 3, { .value = 3 });
	vfm.insert(timeline, 7, vfm.makeValuePtr(4));
	vfm.erase(5);

	{
		auto iter = vfm.find(1);
		CHECK(iter == vfm.end());

		iter = vfm.find(5);
		CHECK(iter == vfm.end());

		iter = vfm.find(3);
		CHECK(iter != vfm.end());
		CHECK(iter->value().value == 3);
	}

	IVFM ivfm1(vfm);

	timeline = 6;
	vfm.insert(timeline, 15, { .value = 5 });
	vfm.erase(3);
	{
		SimpleItem *ptr = vfm.find(timeline, 1);
		CHECK(ptr == nullptr);

		ptr = vfm.find(timeline, 7);
		CHECK(ptr != nullptr);
		ptr->value = 6;
	}

	IVFM ivfm2(vfm, &ivfm1);

	// Check `vfm` contents
	{
		auto iter = vfm.begin();
		CHECK(iter->version() == 6);
		CHECK(iter->key() == 7);
		CHECK(iter->value().value == 6);

		++iter;
		CHECK(iter->version() == 5);
		CHECK(iter->key() == 10);
		CHECK(iter->value().value == 2);

		++iter;
		CHECK(iter->version() == 6);
		CHECK(iter->key() == 15);
		CHECK(iter->value().value == 5);

		++iter;
		CHECK(iter == vfm.end());
	}

	// Check `ivfm1` contents
	{
		auto iter = ivfm1.begin();
		CHECK(iter->version() == 5);
		CHECK(iter->key() == 3);
		CHECK(iter->value().value == 3);

		++iter;
		CHECK(iter->version() == 5);
		CHECK(iter->key() == 7);
		CHECK(iter->value().value == 4);

		++iter;
		CHECK(iter->version() == 5);
		CHECK(iter->key() == 10);
		CHECK(iter->value().value == 2);

		++iter;
		CHECK(iter == ivfm1.end());
	}

	// Check `ivfm2` contents
	{
		auto iter = ivfm2.begin();
		CHECK(iter->version() == 6);
		CHECK(iter->key() == 7);
		CHECK(iter->value().value == 6);

		++iter;
		CHECK(iter->version() == 5);
		CHECK(iter->key() == 10);
		CHECK(iter->value().value == 2);

		++iter;
		CHECK(iter->version() == 6);
		CHECK(iter->key() == 15);
		CHECK(iter->value().value == 5);

		++iter;
		CHECK(iter == ivfm2.end());
	}

	using DiffTuple = std::tuple<uint32_t, const SimpleItem *, const SimpleItem *>;
	std::vector<DiffTuple> diff;
	ivfm2.visitDiff(&ivfm1, [&](uint32_t key, const SimpleItem *new_item, const SimpleItem *old_item) {
		diff.emplace_back(key, new_item, old_item);
		return true;
	});

	CHECK(diff.size() == 3);
	CHECK(diff[0] == DiffTuple { 3, nullptr, ivfm1.find(3)->valueAddr() });
	CHECK(diff[1] == DiffTuple { 7, ivfm2.find(7)->valueAddr(), ivfm1.find(7)->valueAddr() });
	CHECK(diff[2] == DiffTuple { 15, ivfm2.find(15)->valueAddr(), nullptr });
}

TEST_CASE("'V8gFlatMap' storage policies sanity check", "[voxen::v8g_flat_map]")
{
	struct SubItem {
		int values[100];
	};

	struct Item {
		explicit Item(int n, bool copy, bool damage) : allow_copy(copy), allow_damage(damage)
		{
			sub_item = std::make_unique<SubItem>();
			std::iota(std::begin(sub_item->values), std::end(sub_item->values), n);
		}

		Item(const Item &other)
		{
			CHECK(other.allow_copy == true);
			sub_item = std::make_unique<SubItem>(*other.sub_item);
		}

		Item(Item &other)
		{
			CHECK(other.allow_damage == true);
			sub_item = std::move(other.sub_item);
		}

		Item(Item &&) = delete;
		Item &operator=(Item &&) = delete;
		Item &operator=(const Item &) = delete;

		~Item() = default;

		std::unique_ptr<SubItem> sub_item;
		bool allow_copy = false;
		bool allow_damage = false;
	};

	using IVFM = V8gFlatMap<uint32_t, Item, V8gStoragePolicy::Immutable>;

	{
		V8gFlatMap<uint32_t, Item, V8gStoragePolicy::Copyable> cvfm;
		cvfm.insert(1, 1, cvfm.makeValuePtr(10, true, false));

		IVFM ivfm(std::as_const(cvfm));

		auto citer = cvfm.find(1);
		auto iiter = ivfm.find(1);

		CHECK(citer != cvfm.end());
		CHECK(iiter != ivfm.end());
		CHECK(citer->hasValue() == true);
		CHECK(iiter->hasValue() == true);
		// Value must have been copied
		CHECK(citer->valueAddr() != iiter->valueAddr());

		CHECK(iiter->value().sub_item != nullptr);
		CHECK(iiter->value().sub_item->values[0] == 10);
		CHECK(iiter->value().sub_item->values[99] == 109);
	}

	{
		V8gFlatMap<uint32_t, Item, V8gStoragePolicy::DmgCopyable> dvfm;
		dvfm.insert(1, 1, dvfm.makeValuePtr(10, false, true));

		IVFM ivfm(dvfm);

		auto diter = dvfm.find(1);
		auto iiter = ivfm.find(1);

		CHECK(diter != dvfm.end());
		CHECK(iiter != ivfm.end());
		CHECK(diter->hasValue() == true);
		CHECK(iiter->hasValue() == true);
		// Value must have been copied
		CHECK(diter->valueAddr() != iiter->valueAddr());
		// But sub-item must have been moved
		CHECK(diter->value().sub_item == nullptr);

		CHECK(iiter->value().sub_item != nullptr);
		CHECK(iiter->value().sub_item->values[0] == 10);
		CHECK(iiter->value().sub_item->values[99] == 109);
	}

	{
		V8gFlatMap<uint32_t, Item, V8gStoragePolicy::Shared> svfm;
		svfm.insert(1, 1, svfm.makeValuePtr(10, false, false));

		IVFM ivfm(svfm);

		auto siter = svfm.find(1);
		auto iiter = ivfm.find(1);

		CHECK(siter != svfm.end());
		CHECK(iiter != ivfm.end());

		// Value ownership must be shared
		CHECK(siter->valueAddr() == iiter->valueAddr());

		CHECK(iiter->value().sub_item != nullptr);
		CHECK(iiter->value().sub_item->values[0] == 10);
		CHECK(iiter->value().sub_item->values[99] == 109);
	}
}

} // namespace voxen
