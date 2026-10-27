#include <cstdio>
#include <stacktrace>

__declspec(noinline) void f()
{
	auto st = std::stacktrace::current(); // line 6
	for (const auto &e : st)
		std::printf("%u %s\n", e.source_line(), e.description().c_str());
}

__declspec(noinline) void g()
{
	f(); // line 13
	std::puts("g done"); // line 14
}

int main()
{
	g(); // line 19
	std::puts("main done"); // line 20
}
