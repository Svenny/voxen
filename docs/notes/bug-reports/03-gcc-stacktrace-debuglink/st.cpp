#include <cstdio>
#include <stacktrace>

[[gnu::noinline]] void f()
{
	for (const auto &e : std::stacktrace::current())
		std::printf("%s at %s:%u\n", e.description().c_str(), e.source_file().c_str(), e.source_line());
}

int main()
{
	f();
}
