#include <voxen/svc/task_service.hpp>

#include <voxen/svc/engine.hpp>
#include <voxen/svc/task_builder.hpp>

#include "../../voxen_test_common.hpp"

#include <atomic>
#include <thread>
#include <unordered_set>

namespace voxen::svc
{

TEST_CASE("'TaskService' test case 1", "[voxen::svc::task_service]")
{
	auto engine = Engine::create();
	TaskService &ts = engine->serviceLocator().requestService<TaskService>();

	TaskBuilder bld(ts);
	TaskHandle handles[16];

	std::atomic_size_t counter = 0;

	// Launch a bunch of independent tasks
	for (size_t i = 0; i < std::size(handles); i++) {
		handles[i] = bld.enqueueTaskWithHandle([&counter](TaskContext &) { counter.fetch_add(1); });
		CHECK(handles[i].valid());
	}

	for (size_t i = 0; i < std::size(handles); i++) {
		handles[i].wait();
		CHECK(handles[i].finished());
	}

	CHECK(counter.load() == std::size(handles));
}

TEST_CASE("'TaskService' test case 2", "[voxen::svc::task_service]")
{
	auto engine = Engine::create();
	TaskService &ts = engine->serviceLocator().requestService<TaskService>();

	TaskBuilder bld(ts);

	uint64_t last_task_counter = bld.getLastTaskCounter();
	size_t unsafe_counter = 0;
	size_t *unsafe_counter_ptr = &unsafe_counter;

	constexpr size_t NUM_TASKS = 10;

	// Chain some tasks one after another
	for (size_t i = 0; i < NUM_TASKS; i++) {
		bld.addWait(last_task_counter);
		bld.enqueueTask([&unsafe_counter_ptr](TaskContext &) {
			// Remove the shared pointer
			size_t *local_ptr = std::exchange(unsafe_counter_ptr, nullptr);
			if (!local_ptr) {
				// This will break the check below
				return;
			}

			// Wait for some time - if dependency tracking is broken the next task
			// might start in the meantime, will notice there is no pointer and exit
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			++(*local_ptr);

			// Return the shared pointer
			unsafe_counter_ptr = local_ptr;
		});
		last_task_counter = bld.getLastTaskCounter();
	}

	// Wait for completion of the last task
	bld.addWait(last_task_counter);
	TaskHandle sync_handle = bld.enqueueSyncPoint();
	sync_handle.wait();

	CHECK(unsafe_counter == NUM_TASKS);
}

TEST_CASE("'TaskService' test case 3", "[voxen::svc::task_service]")
{
	auto engine = Engine::create();
	TaskService &ts = engine->serviceLocator().requestService<TaskService>();

	constexpr size_t NUM_SPLITS = 64;
	constexpr size_t SPLIT_SIZE = 10'000;

	TaskBuilder bld(ts);

	std::vector<uint64_t> reference_data(NUM_SPLITS * SPLIT_SIZE);
	// First generate data sequentially. Do it as a task too, just for the sake of it.
	bld.enqueueTask([&reference_data](TaskContext &) {
		for (size_t i = 0; i < NUM_SPLITS; i++) {
			size_t begin = i * SPLIT_SIZE;
			size_t end = begin + SPLIT_SIZE;

			std::mt19937_64 rng(0xDEADBEEF + i);

			for (size_t j = begin; j < end; j++) {
				reference_data[j] = rng();
			}
		}
	});

	const uint64_t ref_gen_task_counter = bld.getLastTaskCounter();

	std::vector<uint64_t> data(NUM_SPLITS * SPLIT_SIZE);
	uint64_t gen_task_counters[NUM_SPLITS];

	// Do a kind of "parallel for" to generate the same data
	for (size_t i = 0; i < NUM_SPLITS; i++) {
		size_t begin = i * SPLIT_SIZE;
		size_t end = begin + SPLIT_SIZE;
		uint64_t seed = 0xDEADBEEF + i;

		bld.enqueueTask([begin, end, seed, &data](TaskContext &) {
			std::mt19937_64 rng(seed);

			for (size_t j = begin; j < end; j++) {
				data[j] = rng();
			}
		});

		gen_task_counters[i] = bld.getLastTaskCounter();
	}

	constexpr size_t DIVISOR = 8;

	size_t errors[NUM_SPLITS / DIVISOR] = {};
	uint64_t val_task_counters[NUM_SPLITS / DIVISOR];

	// Now do the second "parallel for" and validate results.
	// Every task will check the results of several generation tasks.
	for (size_t i = 0; i < NUM_SPLITS / DIVISOR; i++) {
		size_t begin = i * SPLIT_SIZE * DIVISOR;
		size_t end = begin + SPLIT_SIZE * DIVISOR;

		size_t &err_counter = errors[i];

		// Wait for the respective generation tasks
		bld.addWait(ref_gen_task_counter);
		bld.addWait(std::span(gen_task_counters + i * DIVISOR, DIVISOR));

		bld.enqueueTask([begin, end, &data, &reference_data, &err_counter](TaskContext &) {
			for (size_t j = begin; j < end; j++) {
				err_counter += (data[j] != reference_data[j]);
			}
		});

		val_task_counters[i] = bld.getLastTaskCounter();
	}

	// Wait for all validation tasks
	bld.addWait(val_task_counters);
	bld.enqueueSyncPoint().wait();

	for (size_t err : errors) {
		CHECK(err == 0);
	}
}

TEST_CASE("'TaskService' test case 4", "[voxen::svc::task_service]")
{
	auto engine = Engine::create();
	TaskService &ts = engine->serviceLocator().requestService<TaskService>();

	TaskBuilder bld(ts);

	size_t unsafe_counter = 0;
	size_t remaining = 15;

	struct TaskObject {
		size_t *counter;
		size_t *remaining;

		void operator()(TaskContext &ctx)
		{
			(*counter)++;
			(*remaining)--;

			if (*remaining > 0) {
				// Issue a new task as a continuation - current task will be considered
				// finished only when the next one (and its subtree) finishes as well.
				TaskBuilder bld(ctx);
				bld.enqueueTask(*this);
			}
		}
	};

	// Launch a recursive chain of continuation tasks.
	// Waiting on the first task must wait for completion of the whole task tree.
	bld
		.enqueueTaskWithHandle(TaskObject {
			.counter = &unsafe_counter,
			.remaining = &remaining,
		})
		.wait();

	CHECK(unsafe_counter == 15);
	CHECK(remaining == 0);
}

TEST_CASE("'TaskService' test case 5", "[voxen::svc::task_service]")
{
	auto engine = Engine::create();
	TaskService &ts = engine->serviceLocator().requestService<TaskService>();

	// Launch a lot of tasks with random dependency graph.
	// Tasks ensure their dependencies have finished and sleep for random times before completing.
	// This stresses `TaskService` counter completion tracking correctness.
	//
	// Buggy implementation can easily hang up (deadlock) the application on this test.
	constexpr size_t NUM_TASKS = 10'000;

	// Accessed from multiple threads without synchronization but there are no races
	std::vector<TaskHandle> task_handles(NUM_TASKS);
	std::vector<uint64_t> task_counters(NUM_TASKS);
	std::atomic_size_t dependency_errors = 0;

	struct TaskObject {
		std::vector<TaskHandle> &task_handles;
		std::atomic_size_t &dependency_errors;

		std::vector<size_t> depends_on;
		uint32_t sleep_usecs;

		void operator()(svc::TaskContext &)
		{
			for (size_t index : depends_on) {
				TaskHandle &handle = task_handles[index];

				if (!handle.finished()) {
					dependency_errors.fetch_add(1);
				}
			}

			// Sleep for a random time before completing
#ifndef _WIN32
			std::this_thread::sleep_for(std::chrono::microseconds(sleep_usecs));
#else
			// Windows sleep is dogshit, doing usec sleep will make this test take hours
			if (sleep_usecs > 140) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			} else {
				std::this_thread::yield();
			}
#endif
		}
	};

	std::mt19937 rng(0xDEADBEEF);
	TaskBuilder bld(ts);

	for (size_t i = 0; i < NUM_TASKS; i++) {
		std::vector<size_t> depends_on;

		if (i > 200) {
			std::uniform_int_distribution<size_t> distr(0, i - 1);
			for (int j = 0; j < 35; j++) {
				depends_on.emplace_back(distr(rng));
				bld.addWait(task_counters[depends_on.back()]);
			}
		}

		TaskObject tobj {
			.task_handles = task_handles,
			.dependency_errors = dependency_errors,
			.depends_on = std::move(depends_on),
			.sleep_usecs = uint32_t(rng() % 150u), // Sleep 0-150 us
		};

		// No race with tasks - this handle can't be accessed by them yet
		task_handles[i] = bld.enqueueTaskWithHandle(std::move(tobj));
		task_counters[i] = bld.getLastTaskCounter();
	}

	// Wait until all tasks complete
	std::vector<bool> completed_bitset(NUM_TASKS);
	size_t num_completed = 0;

	while (num_completed < NUM_TASKS) {
		// Certainly enough to finish at least a few tasks
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
		size_t new_completions = 0;

		for (size_t i = 0; i < NUM_TASKS; i++) {
			if (!completed_bitset[i] && task_handles[i].finished()) {
				completed_bitset[i] = true;
				num_completed++;
				new_completions++;
			}
		}

		if (new_completions == 0) {
			// Unfortunately TaskService destructor will hang waiting for deadlocked threads.
			// Even if it didn't - test execution can't continue after this has happened.
			INFO("TaskService test has deadlocked!");
			abort();
		}
	}

	CHECK(dependency_errors.load() == 0);
}

} // namespace voxen::svc
