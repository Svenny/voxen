#include <voxen/common/scratch_memory_allocator.hpp>

#include "../../voxen_test_common.hpp"

#include <scoped_allocator>

namespace voxen
{

TEST_CASE("'ScratchMemoryAllocator' test case 1", "[voxen::scratch_memory_allocator]")
{
	ScratchMemoryAllocator sma;
	CHECK(sma.allocatedBytes() == 0);

	{
		constexpr size_t N = 12345;

		auto sma1 = sma.scope();

		void *ptr1 = sma1.allocate(N, 256);
		CHECK(uintptr_t(ptr1) % 256 == 0);
		memset(ptr1, 1, N);

		CHECK(sma.allocatedBytes() >= N);
		CHECK(sma.capacityBytes() >= sma.allocatedBytes());

		void *ptr2 = sma1.allocate(N, 64);
		CHECK(uintptr_t(ptr2) % 64 == 0);
		memset(ptr2, 2, N);

		CHECK(sma.allocatedBytes() >= N * 2);
		CHECK(sma.capacityBytes() >= sma.allocatedBytes());

		CHECK(memchr(ptr1, 2, N) == nullptr);
		CHECK(memchr(ptr2, 1, N) == nullptr);
	}

	CHECK(sma.allocatedBytes() == 0);
}

TEST_CASE("'ScratchMemoryAllocator' test case 2", "[voxen::scratch_memory_allocator]")
{
	ScratchMemoryAllocator allocator;

	constexpr size_t N = 12345;

	{
		auto scope1 = allocator.scope();
		CHECK(allocator.allocatedBytes() == 0);

		void *ptr1 = scope1.allocate(N, 8);
		CHECK(allocator.allocatedBytes() >= N);
		memset(ptr1, 1, N);

		{
			auto scope2 = scope1.subscope();
			void *ptr2 = scope2.allocate(N, 8);
			CHECK(allocator.allocatedBytes() >= N + N);
			memset(ptr2, 2, N);

			{
				auto scope3 = scope2.subscope();
				void *ptr3 = scope3.allocate(N, 8);
				CHECK(allocator.allocatedBytes() >= N + N + N);
				memset(ptr3, 3, N);

				CHECK(memchr(ptr2, 3, N) == nullptr);
				CHECK(memchr(ptr1, 3, N) == nullptr);
			}

			CHECK(allocator.allocatedBytes() >= N + N);

			void *ptr4 = scope2.allocate(N, 8);
			memset(ptr4, 4, N);

			CHECK(memchr(ptr2, 4, N) == nullptr);
			CHECK(memchr(ptr1, 4, N) == nullptr);
		}

		CHECK(allocator.allocatedBytes() >= N);

		void *ptr5 = scope1.allocate(N, 8);
		memset(ptr5, 5, N);

		CHECK(memchr(ptr1, 5, N) == nullptr);
	}

	CHECK(allocator.allocatedBytes() == 0);
}

TEST_CASE("'ScratchMemoryAllocator' test case 3", "[voxen::scratch_memory_allocator]")
{
	ScratchMemoryAllocator sma;

	auto inner_scope = [&]() {
		constexpr size_t N = 12345;
		auto scope1 = sma.scope();

		std::vector<void *> ptrs;

		for (size_t i = 0; i < 100; i++) {
			void *ptr = scope1.allocate(N, 8);
			memset(ptr, int(i), N);
			ptrs.emplace_back(ptr);
		}

		CHECK(sma.allocatedBytes() >= ptrs.size() * N);
		CHECK(sma.capacityBytes() >= sma.allocatedBytes());

		for (size_t i = 0; i < ptrs.size(); i++) {
			void *res1 = memchr(ptrs[i], int(i) - 1, N);
			void *res2 = memchr(ptrs[i], int(i) + 1, N);

			// Don't spam assertions count
			if (res1 || res2) {
				CHECK(res1 == nullptr);
				CHECK(res2 == nullptr);
			}
		}
	};

	// Try several times to stress slab fusing
	inner_scope();
	inner_scope();
	inner_scope();
}

TEST_CASE("'ScratchMemoryAllocator' test case 4", "[voxen::scratch_memory_allocator]")
{
	ScratchMemoryAllocator allocator;

	{
		auto scope = allocator.scope();

		using StringAllocator = TScratchMemoryAllocator<char>;
		using String = std::basic_string<char, std::char_traits<char>, StringAllocator>;
		using MapAllocator = TScratchMemoryAllocator<std::pair<const int, String>>;
		using Allocator = std::scoped_allocator_adaptor<MapAllocator, StringAllocator>;
		using Map = std::unordered_map<int, String, std::hash<int>, std::equal_to<int>, Allocator>;

		Map map(Allocator { scope, scope });

		map[1] = "one";
		map[2] = "two";
		map[3] = "three";
		map[-10] = "some-long-string-suppress-small-string-optimization";

		CHECK(map.size() == 4);
		CHECK(map.at(1) == "one");
		CHECK(map.at(2) == "two");
		CHECK(map.at(3) == "three");
		CHECK(map.at(-10) == "some-long-string-suppress-small-string-optimization");

		map.erase(2);
		map.erase(-5);
		CHECK(map.size() == 3);
		CHECK(map.find(2) == map.end());

		size_t pre_scope_bytes = allocator.allocatedBytes();

		{
			auto scope2 = scope.subscope();

			Map map_copy = map;
			CHECK(map_copy.size() == 3);
			CHECK(map_copy.at(1) == "one");
			CHECK(map_copy.at(3) == "three");
			CHECK(map_copy.at(-10) == map.at(-10));
			CHECK(map.at(-10) == "some-long-string-suppress-small-string-optimization");
		}

		CHECK(map.at(1) == "one");
		CHECK(map.at(3) == "three");
		CHECK(map.at(-10) == "some-long-string-suppress-small-string-optimization");

		CHECK(allocator.allocatedBytes() == pre_scope_bytes);
	}

	CHECK(allocator.allocatedBytes() == 0);
}

} // namespace voxen
