#pragma once

// EXPERIMENT: macros can't be exported from modules, they live in a header
// included both in the module's global fragment and by importers needing them.

#ifndef _WIN32
	#define MODSHARED_API __attribute__((visibility("default")))
#else
	#ifdef MODSHARED_EXPORTS
		#define MODSHARED_API __declspec(dllexport)
	#else
		#define MODSHARED_API __declspec(dllimport)
	#endif
#endif

// A "public" macro, importers must include this header to use it
#define MODSHARED_SQUARE(x) ((x) * (x))
