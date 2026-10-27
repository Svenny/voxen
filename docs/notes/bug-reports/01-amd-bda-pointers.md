# AMD Windows driver mishandles PhysicalStorageBuffer pointer values

## Summary

The AMD Windows driver gets `PhysicalStorageBuffer` pointer *values* (GLSL `buffer_reference`)
wrong in two ways. Both modules pass `spirv-val --target-env vulkan1.3`, and every other driver
tested handles them. The same values handled as `uint64_t` work in both cases, so the two bugs likely
share a root cause in how the compiler lowers pointer-typed values (as opposed to loads and stores
*through* pointers, which work).

- **A. Crash:** `vkCreateComputePipelines` crashes inside `amdvlk64.dll` (access violation,
  0xC0000005) when the shader has an `OpPhi` of a pointer type with an `OpUndef` incoming value.
  `glslc -O` produces this shape from plain GLSL where a `buffer_reference` variable is assigned on
  only one path.
- **B. Miscompilation:** `OpStore` of a pointer value into a `StorageBuffer` (SSBO) writes `0`
  instead of the pointer. Applications writing device addresses on the GPU (GPU culling writing
  per-draw vertex buffer addresses, for example) get null pointers; dereferencing them later ends in
  a GPU page fault and `VK_ERROR_DEVICE_LOST` (`VK_EXT_device_fault` reports an invalid read at
  `0x0`-`0xFFF`). Loading a pointer from a storage buffer is not affected.

A's control (`OpUndef` replaced with `OpConvertUToPtr(0)`) creates the pipeline, but on this driver
its pointer store then writes zero (B), so it is not a working workaround at runtime.

## Environment

- AMD Radeon RX 6900 XT (RDNA2)
- AMD Software 26.10.1, `amdvlk64.dll` 32.0.21045.16010, Vulkan `driverVersion` 0x800161,
  `driverName` "AMD proprietary driver", `driverInfo` "26.10.1 (AMD proprietary shader compiler)"
- Windows 11 10.0.26200, x64
- Vulkan SDK 1.4.341.1 (loader/headers), host programs built with MSVC (VS 2026 Build Tools)

## Files

In `01-amd-bda-pointers/`:

- `vk_pipe.c` (A): ~140-line C program. Creates a Vulkan 1.3 instance and device
  (`bufferDeviceAddress` enabled), a pipeline layout with one set of 4 storage buffers, loads a
  `.spv` file and calls `vkCreateComputePipelines` once (no pipeline cache, entry point `main`).
- `vk_store.c` (B): ~220-line C program. Creates a Vulkan 1.3 device (`bufferDeviceAddress`,
  `shaderInt64` enabled), two host-visible storage buffers (input: 256 `uint64_t` values, output:
  256 zero-filled 8-byte slots), runs the given compute shader once (`vkCmdDispatch(4, 1, 1)`, local
  size 64) and checks `output[i] == input[i]`. The values are arbitrary 64-bit patterns, only
  stored, never dereferenced.
- A: `phi_undef_bda.comp` (GLSL), `phi_undef_bda.spvasm` (`glslc -O`, crashes),
  `phi_null_bda.spvasm` (only the `OpUndef` replaced by `OpConvertUToPtr(0)`, doesn't crash),
  `phi_undef_bda.O0.spvasm` (`glslc -O0`, no phi, doesn't crash).
- B: `store_ptr.comp` / `.spvasm` (`uint64_t` in, `buffer_reference` out, fails),
  `store_ptr.O0.spvasm` (the same at `-O0`, fails), `copy_ptr.comp` / `.spvasm` (`buffer_reference`
  in and out, fails), `store_u64.comp` / `.spvasm` (`buffer_reference` in, `uint64_t` out, passes,
  control).

Assemble the binaries with `spirv-as --target-env vulkan1.3 X.spvasm -o X.spv`, or recompile with
`glslc --target-env=vulkan1.3 -O X.comp -o X.spv`.

## A. OpPhi with an OpUndef pointer operand

```glsl
#version 460
#extension GL_EXT_buffer_reference : require

layout(buffer_reference) buffer Ref { uint x; };

layout(local_size_x = 64) in;
layout(set = 0, binding = 0) readonly buffer In { Ref ref[]; } g_in;
layout(set = 0, binding = 1) buffer Out { Ref ref[]; } g_out;

void main()
{
	uint i = gl_GlobalInvocationID.x;
	Ref r;
	bool ok = false;
	if (i < g_in.ref.length()) {
		r = g_in.ref[i];
		ok = true;
	}
	if (ok) {
		g_out.ref[i] = r;
	}
}
```

Relevant part of the `glslc --target-env=vulkan1.3 -O` output (shaderc 2026.1):

```
         %21 = OpTypePointer PhysicalStorageBuffer %24
         %58 = OpUndef %21
...
         %33 = OpLabel
         %41 = OpLoad %21 %40
               OpBranch %34
         %34 = OpLabel
         %57 = OpPhi %21 %58 %5 %41 %33        ; OpUndef incoming from %5
...
               OpStore %52 %57
```

Steps:

```
cl /nologo /W3 vk_pipe.c /I"%VULKAN_SDK%\Include" /link /LIBPATH:"%VULKAN_SDK%\Lib" vulkan-1.lib
vk_pipe.exe phi_undef_bda.spv AMD
vk_pipe.exe phi_null_bda.spv AMD
vk_pipe.exe phi_undef_bda.O0.spv AMD
```

(The second argument picks the first device whose name contains it.) Expected: `OK` for all
three, the undefined value reaches the phi only on the path where it is never used. Actual:

```
> vk_pipe.exe phi_undef_bda.spv AMD
* AMD Radeon RX 6900 XT | AMD proprietary driver | 26.10.1 (AMD proprietary shader compiler) | driverVersion 0x800161
  Intel(R) UHD Graphics | Intel Corporation | 101.7084 | driverVersion 0x195bac
vkCreateComputePipelines...
```

The process exits with 0xC0000005. Windows Application log (event 1000): faulting module
`amdvlk64.dll` 32.0.21045.16010, exception code 0xc0000005, fault offset 0x0000000002323026 (the
same offset on every run, and in the larger application where this was first seen).
`phi_null_bda.spv` and `phi_undef_bda.O0.spv` print `OK`.

Earlier experiments on a larger shader (same driver): a phi of pointers whose inputs are all
defined is fine, an `OpUndef` pointer stored directly (no phi) is fine, `OpConvertUToPtr(OpUndef
%ulong)` as the phi input is fine, and the same phi over `uint64_t` / `uvec2` instead of a pointer
is fine. `VK_KHR_shader_maximal_reconvergence`, a pipeline cache, and creating one pipeline or a
batch made no difference.

## B. Storing a pointer value to a storage buffer writes zero

```glsl
#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(local_size_x = 64) in;
layout(buffer_reference) buffer Ref { uint x; };
layout(set = 0, binding = 0) readonly buffer In { uint64_t v[]; } g_in;
layout(set = 0, binding = 1) writeonly buffer Out { Ref r[]; } g_out;
void main() { uint i = gl_GlobalInvocationID.x; g_out.r[i] = Ref(g_in.v[i]); }
```

Function body of the `glslc --target-env=vulkan1.3 -O` output:

```
         %33 = OpAccessChain %_ptr_StorageBuffer_ulong %30 %int_0 %15
         %34 = OpLoad %ulong %33
         %35 = OpConvertUToPtr %_ptr_PhysicalStorageBuffer__struct_19 %34
         %37 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_19 %22 %int_0 %15
               OpStore %37 %35
```

Steps:

```
cl /nologo /W3 vk_store.c /I"%VULKAN_SDK%\Include" /link /LIBPATH:"%VULKAN_SDK%\Lib" vulkan-1.lib
vk_store.exe store_ptr.spv AMD
vk_store.exe copy_ptr.spv AMD
vk_store.exe store_u64.spv AMD
```

Expected: `0 of 256 values wrong: PASS` for all shaders. Actual:

```
> vk_store.exe store_ptr.spv AMD
* AMD Radeon RX 6900 XT | AMD proprietary driver | 26.10.1 (AMD proprietary shader compiler) | driverVersion 0x800161
  Intel(R) UHD Graphics | Intel Corporation | 101.7084 | driverVersion 0x195bac
  out[0] = 0x0000000000000000, expected 0x0000000312340000
  out[1] = 0x0000000000000000, expected 0x0000000312340040
  out[2] = 0x0000000000000000, expected 0x0000000312340080
  out[3] = 0x0000000000000000, expected 0x00000003123400c0
256 of 256 values wrong: FAIL
```

`copy_ptr.spv` and `store_ptr.O0.spv` give the same output; `store_u64.spv` passes.

Larger experiments on the same driver: the store writes zero whether the destination is a runtime
array of pointers, a pointer member of a struct in a runtime array (scalar or std430 layout), or an
array aliased with `uint`/`float` views of the same binding; whether the pointer value comes from
`OpConvertUToPtr` or from an `OpLoad` of a pointer; with or without `NonReadable`. Non-pointer
members written by the same invocation in the same struct come out right. The zeros are written for
real device addresses of live buffers too, not just for arbitrary patterns.

## Verified

On the machine above (2026-10-25):

| Shader | AMD RX 6900 XT | Intel UHD (101.7084), same machine |
|---|---|---|
| `phi_undef_bda.spv` | crash 0xC0000005 | OK |
| `phi_null_bda.spv` | OK | OK |
| `phi_undef_bda.O0.spv` | OK | OK |
| `store_ptr.spv` | FAIL, all zeros | PASS |
| `store_ptr.O0.spv` | FAIL, all zeros | PASS |
| `copy_ptr.spv` | FAIL, all zeros | PASS |
| `store_u64.spv` | PASS | PASS |

All of them are also fine on Linux with RADV (Mesa 26.2.5), NVIDIA 610.43.02 and lavapipe.

Not checked: other AMD GPUs or older drivers, stores to `PhysicalStorageBuffer` or `Workgroup`
memory, Radeon GPU Analyzer.

## Workaround

Keep device addresses as `uint64_t` wherever they are stored or merged across control flow
(`uint64_t(ref)` / `OpConvertPtrToU` before the store, initialize from a null `uint64_t` address),
and convert to a pointer only where it is dereferenced. `store_u64.spv` is the SPIR-V-level
equivalent for B.
