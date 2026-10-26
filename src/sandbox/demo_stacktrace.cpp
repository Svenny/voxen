// Crashes through `bugFound` from a task service worker thread to show what
// its stack trace looks like: frames of the executable, libvoxen and system libraries,
// recursion, a lambda and inlined frames. Compare with and without split debug info
// (`.debug` files on Linux, PDBs on Windows) and feed printed offsets to `llvm-symbolizer`.

#include <voxen/debug/bug_found.hpp>
#include <voxen/svc/engine.hpp>
#include <voxen/svc/svc_fwd.hpp>
#include <voxen/svc/task_builder.hpp>
#include <voxen/svc/task_service.hpp>

using namespace voxen::svc;

namespace
{

[[gnu::noinline]] void probeFrame(int depth)
{
	if (depth <= 0) {
		voxen::debug::bugFound("stacktrace probe");
	}
	probeFrame(depth - 1);
	// Prevent tail call optimization, keep every recursion level in the trace
	asm volatile("");
}

} // namespace

int main(int argc, char **)
{
	auto engine = Engine::create(EngineStartArgs(AppInfo { .name = "Voxen Stacktrace Demo" }));
	TaskBuilder bld(engine->serviceLocator().requestService<TaskService>());
	// Depth depends on `argc` so the compiler can't fold the recursion
	bld.enqueueTask([argc](TaskContext &) {
		probeFrame(argc + 1);
		asm volatile("");
	});
	bld.enqueueSyncPoint().wait();
	return 0;
}
