#include <cxxabi.h>
#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		int status = 0;
		char *r = abi::__cxa_demangle(argv[i], nullptr, nullptr, &status);
		std::printf("%-55s status=%d %s\n", argv[i], status, r ? r : "(null)");
		std::free(r);
	}
}
