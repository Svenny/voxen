#include <voxen/common/v8g_hash_trie.hpp>
#include <voxen/common/v8g_hash_trie_impl.hpp>

#include <voxen/common/land/chunk_key.hpp>

#include <catch2/catch.hpp>

#include <map>
#include <random>
#include <unordered_map>

namespace voxen
{

namespace
{

struct TrivialKey {
	uint64_t key;

	auto operator<=>(const TrivialKey &other) const = default;
	uint64_t hash() const noexcept { return key; }
};

} // namespace

TEST_CASE("'V8gHashTrie' sanity check", "[voxen::v8g_hash_trie]")
{
	V8gHashTrie<land::ChunkKey, std::string> vht;

	land::ChunkKey ck1(glm::ivec3(0, 0, 0));
	land::ChunkKey ck2(glm::ivec3(0, 0, 0), 1);
	vht.insert(10, ck1, vht.makeValuePtr("lol"));
	vht.insert(10, ck2, vht.makeValuePtr("kek"));

	const auto *item = vht.find(ck1);
	CHECK(item != nullptr);
	CHECK(item->hasValue());
	CHECK(item->value() == "lol");

	item = vht.find(ck2);
	CHECK(item != nullptr);
	CHECK(item->hasValue());
	CHECK(item->value() == "kek");
}

TEST_CASE("'V8gHashTrie' under random insertions", "[voxen::v8g_hash_trie]")
{
	V8gHashTrie<TrivialKey, uint64_t> vht;
	std::unordered_map<uint64_t, uint64_t> verification;

	std::mt19937_64 rng(0xDEADBEEF);

	for (uint64_t timeline = 0; timeline < 10; timeline++) {
		for (int j = 0; j < 2000; j++) {
			uint64_t key = rng();
			uint64_t value = rng();

			vht.insert(timeline, TrivialKey(key), vht.makeValuePtr(value));
			verification[key] = value;
		}
	}

	size_t found_items = 0;
	size_t correct_items = 0;

	for (const auto &[key, value] : verification) {
		const auto *item = vht.find(TrivialKey(key));

		if (item != nullptr) {
			found_items++;

			if (item->value() == value) {
				correct_items++;
			}
		}

		// Erase visited items to additionally stress that path
		vht.erase(10, TrivialKey(key));
	}

	CHECK(found_items == verification.size());
	CHECK(correct_items == verification.size());
}

TEST_CASE("'V8gHashTrie' iteration", "[voxen::v8g_hash_trie]")
{
	V8gHashTrie<TrivialKey, uint64_t> vht;
	std::map<uint64_t, uint64_t> verification;

	std::mt19937_64 rng(0xDEADBEEF + 1);

	for (uint64_t timeline = 0; timeline < 10; timeline++) {
		for (int j = 0; j < 2000; j++) {
			uint64_t key = rng();
			uint64_t value = rng();

			vht.insert(timeline, TrivialKey(key), vht.makeValuePtr(value));
			verification[key] = value;
		}
	}

	CHECK(vht.size() == verification.size());

	// `std::map` sorts by key. Hashes are equal to keys so `vht` must have the same order.
	const auto *item = vht.findFirst();
	auto iter = verification.begin();

	size_t correct = 0;

	while (item != nullptr) {
		uint64_t ik = item->key().key;
		uint64_t tk = iter->first;

		uint64_t iv = item->value();
		uint64_t tv = iter->second;

		correct += ik == tk && iv == tv;

		item = vht.findNext(item->key());
		++iter;
	}

	CHECK(correct == verification.size());
}

TEST_CASE("'V8gHashTrie' diff", "[voxen::v8g_hash_trie]")
{
	enum Action : int {
		Add = 0,
		Retain = 1,
		Modify = 2,
		Remove = 3,
	};

	std::unordered_map<uint64_t, std::pair<uint64_t, int>> data;

	size_t expected_add = 0;
	size_t expected_modify = 0;
	size_t expected_remove = 0;

	std::mt19937_64 rng(0xDEADBEEF + 2);
	for (size_t i = 0; i < 25'000; i++) {
		uint64_t key = rng();
		uint64_t value = rng();
		int action = rng() % 4;

		data[key] = std::make_pair(value, action);
		expected_add += action == Add;
		expected_modify += action == Modify;
		expected_remove += action == Remove;
	}

	// Insert items for Retain/Remove/Modify actions with version 1
	V8gHashTrie<TrivialKey, uint64_t> vht;

	for (const auto &[key, va] : data) {
		if (va.second != Add) {
			vht.insert(1, TrivialKey(key), vht.makeValuePtr(va.first));
		}
	}

	// Make a snapshot, then perform Add/Modify/Remove actions with version 2
	V8gHashTrie<TrivialKey, uint64_t> snapshot1 = vht;

	for (const auto &[key, va] : data) {
		if (va.second == Add) {
			vht.insert(2, TrivialKey(key), vht.makeValuePtr(va.first));
		} else if (va.second == Modify) {
			// Invert bits
			vht.insert(2, TrivialKey(key), vht.makeValuePtr(~va.first));
		} else if (va.second == Remove) {
			vht.erase(2, TrivialKey(key));
		}
	}

	// Make another snapshot, just for the sake of it
	V8gHashTrie<TrivialKey, uint64_t> snapshot2 = vht;

	CHECK(snapshot1.size() == data.size() - expected_add);
	CHECK(snapshot2.size() == data.size() - expected_remove);

	using Item = V8gHashTrie<TrivialKey, uint64_t>::Item;

	size_t found_add = 0;
	size_t found_modify = 0;
	size_t found_remove = 0;

	// Diff snapshots, validate against `data`
	snapshot2.visitDiff(snapshot1, [&](const Item *new_item, const Item *old_item) {
		// Guard REQUIRE clauses with ifs to not bloat assertions count

		if (new_item && old_item) {
			if (new_item->key() != old_item->key()) {
				REQUIRE(new_item->key() == old_item->key());
			}

			if (data[new_item->key().key].second != Modify) {
				REQUIRE(data[new_item->key().key].second == Modify);
			}

			if (new_item->value() != ~old_item->value()) {
				REQUIRE(new_item->value() == ~old_item->value());
			}

			found_modify++;
			data.erase(new_item->key().key);
		} else if (new_item) {
			auto va = data[new_item->key().key];

			if (va.second != Add) {
				REQUIRE(va.second == Add);
			}

			if (va.first != new_item->value()) {
				REQUIRE(va.first == new_item->value());
			}

			found_add++;
			data.erase(new_item->key().key);
		} else if (old_item) {
			auto va = data[old_item->key().key];

			if (va.second != Remove) {
				REQUIRE(va.second == Remove);
			}

			if (va.first != old_item->value()) {
				REQUIRE(va.first == old_item->value());
			}

			found_remove++;
			data.erase(old_item->key().key);
		}
		return true;
	});

	CHECK(found_add == expected_add);
	CHECK(found_modify == expected_modify);
	CHECK(found_remove == expected_remove);
}

} // namespace voxen
