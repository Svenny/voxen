#include <voxen/common/uid.hpp>

#include <charconv>
#include <cstdio>
#include <cstring>

int main(int argc, char *argv[])
{
	int count = 1;

	if (argc > 1) {
		count = -1;
		std::from_chars(argv[1], argv[1] + strlen(argv[1]), count);
	}

	if (count < 1) {
		printf("Usage: %s [n]\n", argv[0]);
		puts("n is the number of UIDs to generate (1 by default)");
		return 0;
	}

	for (int i = 0; i < count; i++) {
		auto uid = voxen::UID::generateRandom();

		char buf[voxen::UID::CHAR_REPR_LENGTH];
		uid.toChars(buf);
		puts(buf);
	}

	return 0;
}
