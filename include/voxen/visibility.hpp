#pragma once

// Denotes public symbol (exported from shared library)
#define VOXEN_API __attribute__((visibility("default")))

// Denotes private symbol (not visible outside shared library)
#define VOXEN_LOCAL __attribute__((visibility("hidden")))
