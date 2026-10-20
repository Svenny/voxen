#pragma once

// Shared library visibility macros:
//
// VOXEN_API - public symbol (exported from shared library)
// VOXEN_LOCAL - private symbol (not visible outside shared library)
// VOXEN_EXTERN_TEMPLATE_API - for `extern template` declarations of explicit
//     instantiations exported with `template class VOXEN_API ...` definitions
//
// We build most (if not all) code with `-fvisibility=hidden`
// so the latter is usually not needed. Use it only to hide
// some private stuff declared in exported classes for example.

#ifndef _WIN32
	#define VOXEN_API __attribute__((visibility("default")))
	#define VOXEN_LOCAL __attribute__((visibility("hidden")))
	// Must be on the declaration too, consumers' references would be hidden otherwise
	#define VOXEN_EXTERN_TEMPLATE_API VOXEN_API
#else
	#ifdef VOXEN_EXPORTS
		#define VOXEN_API __declspec(dllexport)
		// `dllexport` goes on the instantiation definition only,
		// clang warns about it on the declaration (`-Wdllexport-explicit-instantiation-decl`)
		#define VOXEN_EXTERN_TEMPLATE_API
	#else
		#define VOXEN_API __declspec(dllimport)
		#define VOXEN_EXTERN_TEMPLATE_API __declspec(dllimport)
	#endif
	#define VOXEN_LOCAL
#endif
