#include <voxen/log.hpp>
#include <voxen/svc/engine.hpp>
#include <voxen/svc/svc_fwd.hpp>
#include <voxen/svc/task_builder.hpp>
#include <voxen/svc/task_coro.hpp>
#include <voxen/svc/task_service.hpp>

#include <tk/rng.hpp>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ratio>
#include <vector>

using namespace voxen::svc;

namespace
{

float doTask(int iters) noexcept
{
	float v = 1.5f;
	for (int i = 0; i < iters; i++) {
		v = std::sin(1.25f * v - 0.75f) + std::cos(1.75f * v - 0.25f);
	}
	return v;
}

CoroTask coroTask(float *output, int iters)
{
	*output = doTask(iters);
	co_return;
}

} // namespace

int main()
{
	auto engine = Engine::create(EngineStartArgs(AppInfo { .name = "Voxen Coroutines Demo" }));
	TaskBuilder bld(engine->serviceLocator().requestService<TaskService>());

	constexpr size_t NUM_BATCHES = 1000;
	constexpr size_t BATCH_SIZE = 50'000;

	std::vector<uint64_t> counters(BATCH_SIZE);
	std::vector<float> output_values(BATCH_SIZE);
	std::vector<int> iters_values(BATCH_SIZE);

	tk::rng::Pcg64 rng(42);

	double sum_regular = 0.0;
	double sum_coro = 0.0;

	for (size_t batch = 0; batch < NUM_BATCHES; batch++) {
		// Pre-generate values to be the same for both coroutine and regular tasks
		for (size_t i = 0; i < BATCH_SIZE; i++) {
			iters_values[i] = tk::rng::uniformInt(rng, 1, 50);
		}

		auto t1 = std::chrono::steady_clock::now();

		// Regular task launch
		for (size_t i = 0; i < BATCH_SIZE; i++) {
			bld.enqueueTask(
				[output = &output_values[i], iters = iters_values[i]](TaskContext &) { *output = doTask(iters); });
			counters[i] = bld.getLastTaskCounter();
		}

		bld.addWait(counters);
		bld.enqueueSyncPoint().wait();

		auto t2 = std::chrono::steady_clock::now();

		// Coroutine task launch
		for (size_t i = 0; i < BATCH_SIZE; i++) {
			bld.enqueueTask(coroTask(&output_values[i], iters_values[i]));
			counters[i] = bld.getLastTaskCounter();
		}

		bld.addWait(counters);
		bld.enqueueSyncPoint().wait();

		auto t3 = std::chrono::steady_clock::now();

		double elapsed_regular = std::chrono::duration<double, std::milli>(t2 - t1).count();
		double elapsed_coro = std::chrono::duration<double, std::milli>(t3 - t2).count();

		sum_regular += elapsed_regular;
		sum_coro += elapsed_coro;

		if (batch % 50 == 0) {
			double ravg = sum_regular / double(batch + 1);
			double cavg = sum_coro / double(batch + 1);
			double diff = (cavg - ravg) * 1'000'000.0 / double(BATCH_SIZE);
			voxen::Log::info("R {} ms, C {} ms, Ravg {} ms, Cavg {} ms, D {} ns", elapsed_regular, elapsed_coro, ravg,
				cavg, diff);
			fflush(stdout);
		}
	}

	return 0;
}
