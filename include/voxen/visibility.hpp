#pragma once

// Shared library visibility macros:
//
// VOXEN_API - public symbol (exported from shared library)
// VOXEN_LOCAL - private symbol (not visible outside shared library)
//
// We build most (if not all) code with `-fvisibility=hidden`
// so the latter is usually not needed. Use it only to hide
// some private stuff declared in exported classes for example.

#ifndef _WIN32
	#define VOXEN_API __attribute__((visibility("default")))
	#define VOXEN_LOCAL __attribute__((visibility("hidden")))
#else
	#ifdef VOXEN_EXPORTS
		#define VOXEN_API __declspec(dllexport)
	#else
		#define VOXEN_API __declspec(dllimport)
	#endif
	#define VOXEN_LOCAL
#endif
