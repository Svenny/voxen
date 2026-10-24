// GPU data layout shared between shaders and C++, compiled by both slangc and the C++ compiler.
// Keep to the common subset: plain structs of scalar/vector/matrix types with HLSL names.
// Slang `#include`s it into entry point modules (declarations are module-internal there),
// C++ includes it via `src/voxen/gfx/vk/shader_interop.hpp`, which maps the type names to `m`.
// Layout is the same in C++ and in scalar/std430 GPU layout: no implicit padding anywhere.

// Main scene constants, uniform buffer at `set = 0, binding = 0` of land passes
struct MainSceneUbo {
	float4x4 translated_world_to_clip;
	float3 world_position;
	float _pad0;
};
