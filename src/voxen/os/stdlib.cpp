#include <voxen/os/stdlib.hpp>

#ifndef _WIN32
	#include <sys/stat.h>
	#include <unistd.h>
#else
	#include <io.h>
#endif

namespace voxen::os
{

int64_t Stdlib::fileSize(FILE* stream)
{
#ifndef _WIN32
	struct stat file_stat;
	if (fstat(fileno(stream), &file_stat) != 0) {
		return -1;
	}

	return static_cast<int64_t>(file_stat.st_size);
#else
	return _filelengthi64(_fileno(stream));
#endif
}

} // namespace voxen::os
