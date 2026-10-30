// Micro-benchmark of `V8gHashTrie` as a per-component ECS store against a paged
// copy-on-write baseline.
//
// Workload: `--items` components of 64 bytes keyed by a dense 64-bit index. Every tick
// updates `--updates` random keys, publishes a snapshot (a copy of the container) and
// retires the oldest one so that `--retain` snapshots stay alive. The diff between the
// two newest snapshots is measured separately from the tick.
//
// Reports per tick: wall time (update, publish+retire, mean and worst), heap allocations
// and bytes, bytes a retired snapshot was holding alone, peak RSS of the scenario.
// Linux only: allocation accounting replaces the global `operator new` and relies on
// glibc `malloc_usable_size()`, peak RSS comes from `/proc/self`.

#include <voxen/util/v8g_hash_trie.hpp>
#include <voxen/util/v8g_hash_trie_impl.hpp>

#include <tk/cli.hpp>
#include <tk/hash.hpp>

#include <fmt/format.h>

#include <malloc.h>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <memory>
#include <new>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace
{

// --- Allocation accounting -----------------------------------------------------------

std::atomic<uint64_t> g_alloc_count = 0;
std::atomic<uint64_t> g_alloc_bytes = 0;
std::atomic<uint64_t> g_live_bytes = 0;

struct AllocCounters {
	uint64_t count = 0;
	uint64_t bytes = 0;
	uint64_t live = 0;

	static AllocCounters read() noexcept
	{
		return {
			.count = g_alloc_count.load(std::memory_order_relaxed),
			.bytes = g_alloc_bytes.load(std::memory_order_relaxed),
			.live = g_live_bytes.load(std::memory_order_relaxed),
		};
	}
};

void accountAlloc(void *ptr) noexcept
{
	size_t usable = malloc_usable_size(ptr);
	g_alloc_count.fetch_add(1, std::memory_order_relaxed);
	g_alloc_bytes.fetch_add(usable, std::memory_order_relaxed);
	g_live_bytes.fetch_add(usable, std::memory_order_relaxed);
}

void accountFree(void *ptr) noexcept
{
	g_live_bytes.fetch_sub(malloc_usable_size(ptr), std::memory_order_relaxed);
}

void *trackedAlloc(size_t size)
{
	void *ptr = std::malloc(size > 0 ? size : 1);
	if (!ptr) {
		throw std::bad_alloc();
	}
	accountAlloc(ptr);
	return ptr;
}

void *trackedAlignedAlloc(size_t size, std::align_val_t align)
{
	size_t alignment = static_cast<size_t>(align);
	size_t rounded = (size + alignment - 1) / alignment * alignment;
	void *ptr = std::aligned_alloc(alignment, rounded > 0 ? rounded : alignment);
	if (!ptr) {
		throw std::bad_alloc();
	}
	accountAlloc(ptr);
	return ptr;
}

void trackedFree(void *ptr) noexcept
{
	if (ptr) {
		accountFree(ptr);
		std::free(ptr);
	}
}

// --- Peak RSS ------------------------------------------------------------------------

void resetPeakRss()
{
	// Resets the high water mark, then the next `VmHWM` read is the peak since this point
	std::ofstream("/proc/self/clear_refs") << "5\n";
	// Give freed heap back so the next scenario's peak is its own, not the previous one's
	malloc_trim(0);
}

uint64_t readPeakRssBytes()
{
	std::ifstream status("/proc/self/status");
	std::string line;
	while (std::getline(status, line)) {
		if (line.starts_with("VmHWM:")) {
			uint64_t kib = 0;
			std::string_view rest = std::string_view(line).substr(6);
			while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
				rest.remove_prefix(1);
			}
			std::from_chars(rest.data(), rest.data() + rest.size(), kib);
			return kib * 1024;
		}
	}
	return 0;
}

// --- Workload types ------------------------------------------------------------------

// Dense 64-bit key: the trie demands collision-free 64-bit hashes,
// `xxh64Fixed` is a bijection on `uint64_t` (see `land::ChunkKey`).
struct DenseKey {
	uint64_t index = 0;

	bool operator==(const DenseKey &) const noexcept = default;
	auto operator<=>(const DenseKey &) const noexcept = default;

	uint64_t hash() const noexcept { return tk::hash::xxh64Fixed(index); }
};

static_assert(voxen::CV8gUniqueHashableKey<DenseKey>);

// A 64-byte plain component, about the size of a transform
struct Component {
	uint64_t words[8];
};

static_assert(sizeof(Component) == 64);

Component makeComponent(uint64_t index, uint64_t tick) noexcept
{
	Component c;
	for (uint64_t &w : c.words) {
		w = index ^ tick;
	}
	return c;
}

// `V8gHashTrie` with one `shared_ptr`-owned value per component
class TrieStore {
public:
	using Trie = voxen::V8gHashTrie<DenseKey, Component>;
	using Snapshot = Trie;

	static constexpr std::string_view NAME = "hash trie";

	void fill(uint64_t tick, size_t items)
	{
		for (size_t i = 0; i < items; i++) {
			m_head.insert(tick, DenseKey { i }, Trie::makeValuePtr(makeComponent(i, tick)));
		}
	}

	void update(uint64_t tick, uint64_t index)
	{
		m_head.insert(tick, DenseKey { index }, Trie::makeValuePtr(makeComponent(index, tick)));
	}

	Snapshot snapshot() const { return m_head; }

	// Number of items `visitDiff` reports between two snapshots
	static size_t diff(const Snapshot &newer, const Snapshot &older)
	{
		size_t changed = 0;
		newer.visitDiff(older, [&](const Trie::Item *, const Trie::Item *) {
			changed++;
			return true;
		});
		return changed;
	}

private:
	Trie m_head;
};

// Baseline: components in pages of `PAGE_ITEMS`, a page is copied on its first write
// in a tick, a snapshot is the vector of page pointers. Each page also carries per-item
// versions so that a diff can name changed items, like the trie's `Item::version()`.
class PagedStore {
public:
	static constexpr size_t PAGE_ITEMS = 1024;

	struct Page {
		Component items[PAGE_ITEMS];
		uint64_t versions[PAGE_ITEMS];
	};

	using PagePtr = std::shared_ptr<Page>;
	using Snapshot = std::vector<PagePtr>;

	static constexpr std::string_view NAME = "paged COW";

	void fill(uint64_t tick, size_t items)
	{
		size_t pages = (items + PAGE_ITEMS - 1) / PAGE_ITEMS;
		m_pages.resize(pages);
		m_page_tick.assign(pages, tick);

		for (size_t p = 0; p < pages; p++) {
			m_pages[p] = std::make_shared<Page>();
			for (size_t j = 0; j < PAGE_ITEMS; j++) {
				m_pages[p]->items[j] = makeComponent(p * PAGE_ITEMS + j, tick);
				m_pages[p]->versions[j] = tick;
			}
		}
	}

	void update(uint64_t tick, uint64_t index)
	{
		size_t p = index / PAGE_ITEMS;
		if (m_page_tick[p] != tick) {
			m_pages[p] = std::make_shared<Page>(*m_pages[p]);
			m_page_tick[p] = tick;
		}

		size_t j = index % PAGE_ITEMS;
		m_pages[p]->items[j] = makeComponent(index, tick);
		m_pages[p]->versions[j] = tick;
	}

	Snapshot snapshot() const { return m_pages; }

	static size_t diff(const Snapshot &newer, const Snapshot &older)
	{
		size_t changed = 0;
		for (size_t p = 0; p < newer.size(); p++) {
			if (newer[p] == older[p]) {
				continue;
			}
			for (size_t j = 0; j < PAGE_ITEMS; j++) {
				if (newer[p]->versions[j] != older[p]->versions[j]) {
					changed++;
				}
			}
		}
		return changed;
	}

private:
	std::vector<PagePtr> m_pages;
	std::vector<uint64_t> m_page_tick;
};

// --- Driver --------------------------------------------------------------------------

struct Options {
	size_t items = 1'000'000;
	std::vector<size_t> updates = { 10'000, 100'000, 500'000 };
	std::vector<size_t> retains = { 1, 4, 64 };
	size_t warmup = 4;
	size_t ticks = 24;
	uint64_t seed = 0xC0FFEE;
	bool run_trie = true;
	bool run_paged = true;
};

struct Result {
	std::string_view structure;
	size_t updates = 0;
	size_t retain = 0;
	double update_ms = 0;
	double publish_ms = 0;
	double tick_ms = 0;
	double tick_max_ms = 0;
	double allocs = 0;
	double alloc_mib = 0;
	double retained_mib = 0;
	double diff_ms = 0;
	double diff_items = 0;
	double peak_rss_mib = 0;
};

using Clock = std::chrono::steady_clock;

double msBetween(Clock::time_point a, Clock::time_point b) noexcept
{
	return std::chrono::duration<double, std::milli>(b - a).count();
}

double toMib(uint64_t bytes) noexcept
{
	return double(bytes) / double(1 << 20);
}

template<typename Store>
Result runScenario(const Options &opts, size_t updates, size_t retain)
{
	resetPeakRss();

	Store store;
	uint64_t tick = 1;

	AllocCounters fill_begin = AllocCounters::read();
	Clock::time_point fill_t0 = Clock::now();
	store.fill(tick, opts.items);
	Clock::time_point fill_t1 = Clock::now();
	AllocCounters fill_end = AllocCounters::read();

	fmt::print(stderr, "{} / {} updates / retain {}: fill {:.1f} ms, {} allocs, {:.1f} MiB live\n", Store::NAME, updates,
		retain, msBetween(fill_t0, fill_t1), fill_end.count - fill_begin.count, toMib(fill_end.live));

	std::deque<typename Store::Snapshot> ring;
	ring.push_back(store.snapshot());

	std::vector<uint64_t> indices(updates);
	std::mt19937_64 rng(opts.seed);

	Result result;
	result.structure = Store::NAME;
	result.updates = updates;
	result.retain = retain;

	// The ring reaches steady state (a snapshot retired every tick) only after `retain` ticks
	size_t warmup = std::max(opts.warmup, retain);
	size_t measured = 0;

	for (size_t t = 0; t < warmup + opts.ticks; t++) {
		tick++;
		for (uint64_t &index : indices) {
			index = rng() % opts.items;
		}

		AllocCounters before = AllocCounters::read();
		Clock::time_point t0 = Clock::now();

		for (uint64_t index : indices) {
			store.update(tick, index);
		}

		Clock::time_point t1 = Clock::now();
		ring.push_back(store.snapshot());
		Clock::time_point t2 = Clock::now();

		size_t changed = Store::diff(ring[ring.size() - 1], ring[ring.size() - 2]);

		Clock::time_point t3 = Clock::now();
		AllocCounters before_retire = AllocCounters::read();
		while (ring.size() > retain) {
			ring.pop_front();
		}
		Clock::time_point t4 = Clock::now();
		AllocCounters after = AllocCounters::read();

		if (t < warmup) {
			continue;
		}

		measured++;
		double update_ms = msBetween(t0, t1);
		double publish_ms = msBetween(t1, t2) + msBetween(t3, t4);
		result.update_ms += update_ms;
		result.publish_ms += publish_ms;
		result.tick_ms += update_ms + publish_ms;
		result.tick_max_ms = std::max(result.tick_max_ms, update_ms + publish_ms);
		result.allocs += double(after.count - before.count);
		result.alloc_mib += toMib(after.bytes - before.bytes);
		result.retained_mib += toMib(before_retire.live - after.live);
		result.diff_ms += msBetween(t2, t3);
		result.diff_items += double(changed);
	}

	double n = double(measured);
	result.update_ms /= n;
	result.publish_ms /= n;
	result.tick_ms /= n;
	result.allocs /= n;
	result.alloc_mib /= n;
	result.retained_mib /= n;
	result.diff_ms /= n;
	result.diff_items /= n;
	result.peak_rss_mib = toMib(readPeakRssBytes());

	fmt::print(stderr, "  tick {:.2f} ms, {:.1f} MiB live at steady state, peak RSS {:.0f} MiB\n", result.tick_ms,
		toMib(AllocCounters::read().live), result.peak_rss_mib);

	return result;
}

void printTable(const std::vector<Result> &results)
{
	fmt::print(
		"| structure | updates | retain | tick ms | worst ms | update ms | publish+retire ms | allocs/tick | "
		"alloc MiB/tick | retained MiB/snapshot | diff ms | diff items | peak RSS MiB |\n");
	fmt::print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");

	for (const Result &r : results) {
		fmt::print(
			"| {} | {} | {} | {:.2f} | {:.2f} | {:.2f} | {:.2f} | {:.0f} | {:.1f} | {:.1f} | {:.2f} | {:.0f} | {:.0f} |\n",
			r.structure, r.updates, r.retain, r.tick_ms, r.tick_max_ms, r.update_ms, r.publish_ms, r.allocs, r.alloc_mib,
			r.retained_mib, r.diff_ms, r.diff_items, r.peak_rss_mib);
	}
}

std::vector<size_t> parseList(std::string_view text)
{
	std::vector<size_t> values;
	while (!text.empty()) {
		size_t comma = text.find(',');
		std::string_view part = text.substr(0, comma);
		size_t value = 0;
		auto [ptr, ec] = std::from_chars(part.data(), part.data() + part.size(), value);
		if (ec != std::errc() || ptr != part.data() + part.size() || value == 0) {
			throw std::invalid_argument(fmt::format("bad list element '{}'", part));
		}
		values.push_back(value);
		text = comma == std::string_view::npos ? std::string_view() : text.substr(comma + 1);
	}
	return values;
}

} // namespace

// --- Global allocation hooks ---------------------------------------------------------

void *operator new(size_t size)
{
	return trackedAlloc(size);
}

void *operator new[](size_t size)
{
	return trackedAlloc(size);
}

void *operator new(size_t size, const std::nothrow_t &) noexcept
{
	try {
		return trackedAlloc(size);
	}
	catch (const std::bad_alloc &) {
		return nullptr;
	}
}

void *operator new[](size_t size, const std::nothrow_t &) noexcept
{
	try {
		return trackedAlloc(size);
	}
	catch (const std::bad_alloc &) {
		return nullptr;
	}
}

void *operator new(size_t size, std::align_val_t align)
{
	return trackedAlignedAlloc(size, align);
}

void *operator new[](size_t size, std::align_val_t align)
{
	return trackedAlignedAlloc(size, align);
}

void operator delete(void *ptr) noexcept
{
	trackedFree(ptr);
}

void operator delete[](void *ptr) noexcept
{
	trackedFree(ptr);
}

void operator delete(void *ptr, size_t) noexcept
{
	trackedFree(ptr);
}

void operator delete[](void *ptr, size_t) noexcept
{
	trackedFree(ptr);
}

void operator delete(void *ptr, std::align_val_t) noexcept
{
	trackedFree(ptr);
}

void operator delete[](void *ptr, std::align_val_t) noexcept
{
	trackedFree(ptr);
}

void operator delete(void *ptr, size_t, std::align_val_t) noexcept
{
	trackedFree(ptr);
}

void operator delete[](void *ptr, size_t, std::align_val_t) noexcept
{
	trackedFree(ptr);
}

int main(int argc, char **argv)
{
	using tk::cli::Parser;
	using ValueType = Parser::ValueType;

	Parser parser("bench-v8g", "V8gHashTrie vs paged copy-on-write as a per-component ECS store");
	parser.addFlag("help", 'h', "Print this help");
	auto add_option = [&](std::string name, ValueType type, std::string description, std::string default_value) {
		parser.addOption({
			.name = std::move(name),
			.short_name = 0,
			.type = type,
			.description = std::move(description),
			.default_value = std::move(default_value),
			.group = {},
		});
	};
	add_option("items", ValueType::Integer, "Components in the store", "1000000");
	add_option("updates", ValueType::String, "Comma-separated list of random updates per tick", "10000,100000,500000");
	add_option("retain", ValueType::String, "Comma-separated list of retained snapshot counts", "1,4,64");
	add_option("warmup", ValueType::Integer, "Unmeasured ticks per scenario, at least the retained count", "4");
	add_option("ticks", ValueType::Integer, "Measured ticks per scenario", "24");
	add_option("structure", ValueType::String, "trie, paged or both", "both");
	add_option("seed", ValueType::Integer, "Random key sequence seed", "12648430");

	auto parsed = parser.parse(argc, argv);
	if (!parsed) {
		fmt::print(stderr, "{}\n\n{}", parsed.error(), parser.helpText());
		return 2;
	}
	if (parsed->flag("help")) {
		fmt::print("{}", parser.helpText());
		return 0;
	}

	Options opts;
	try {
		opts.items = size_t(*parsed->integer("items"));
		opts.updates = parseList(*parsed->value("updates"));
		opts.retains = parseList(*parsed->value("retain"));
		opts.warmup = size_t(*parsed->integer("warmup"));
		opts.ticks = size_t(*parsed->integer("ticks"));
		opts.seed = uint64_t(*parsed->integer("seed"));

		std::string_view structure = *parsed->value("structure");
		opts.run_trie = structure == "both" || structure == "trie";
		opts.run_paged = structure == "both" || structure == "paged";
		if (!opts.run_trie && !opts.run_paged) {
			throw std::invalid_argument(fmt::format("unknown structure '{}'", structure));
		}
		if (opts.items == 0 || opts.ticks == 0) {
			throw std::invalid_argument("items and ticks must be positive");
		}
	}
	catch (const std::invalid_argument &err) {
		fmt::print(stderr, "{}\n\n{}", err.what(), parser.helpText());
		return 2;
	}

	fmt::print("items: {}, warmup ticks: max({}, retain), measured ticks: {}, component: {} bytes, page: {} items\n\n",
		opts.items, opts.warmup, opts.ticks, sizeof(Component), PagedStore::PAGE_ITEMS);

	std::vector<Result> results;

	for (size_t updates : opts.updates) {
		for (size_t retain : opts.retains) {
			if (opts.run_trie) {
				results.push_back(runScenario<TrieStore>(opts, updates, retain));
			}
			if (opts.run_paged) {
				results.push_back(runScenario<PagedStore>(opts, updates, retain));
			}
		}
	}

	fmt::print("\n");
	printTable(results);
	return 0;
}
