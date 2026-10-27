# vkCreateComputePipelines access violation when OpName targets a NonSemantic OpExtInst (breaks Slang `-g2` shaders)

## Summary

`vkCreateComputePipelines` crashes inside `amdvlk64.dll` (access violation, 0xC0000005) when the
module has an `OpName` whose target is the result of an `OpExtInst` from a `NonSemantic.*`
instruction set. One `NonSemantic.Shader.DebugInfo.100` `DebugSource` plus
`OpName %src "src"` is enough; an unknown set `NonSemantic.Foo.1` crashes the same way. The same
modules without that `OpName` work, and full glslang `-gV`/`-gVS` debug info (which names no
debug instructions) works.

Slang `-g2` emits such an `OpName` on `DebugLocalVariable`s (here `OpName %35 "id"` for the
entry-point parameter), so Slang shaders compiled with `-g2` that have named locals or parameters
crash pipeline creation on this driver. Likely the same as
https://github.com/shader-slang/slang/issues/8051 (RX 6700, driver 25.10, closed there as a driver
issue without a root cause).

`spirv-val --target-env vulkan1.3` accepts all the modules below. Note that
SPV_KHR_non_semantic_info says "Result IDs from these non-semantic instruction set instructions
must only be be used in other non-semantic instructions" and that the instructions "can be safely
removed from the module"; whether `OpName` counts as a use is not spelled out. Either way a
consumer should not crash; ignoring the name (or the whole instruction) would be fine.

## Environment

- AMD Radeon RX 6900 XT (RDNA2)
- AMD Software 26.10.1, `amdvlk64.dll` 32.0.21045.16010, Vulkan `driverVersion` 0x800161,
  `driverInfo` "26.10.1 (AMD proprietary shader compiler)"
- Windows 11 10.0.26200, x64
- Vulkan SDK 1.4.341.1 (loader/headers), host program built with MSVC (VS 2026 Build Tools)

## Files

- `02-amd-opname-nonsemantic/vk_pipe.c`: ~140-line C program. Creates a Vulkan 1.3 instance and device,
  a pipeline layout with one set of 4 storage buffers, loads a `.spv` file and calls
  `vkCreateComputePipelines` once (no pipeline cache, entry point `main`).
- `A_debugsource_named.spvasm` / `.spv`: minimal crashing module (below).
- `B_debugsource_unnamed`: A without the `OpName %src` line (OK).
- `C_debugtypebasic_named`: `OpName` on a `DebugTypeBasic` instead (crash).
- `D_unknown_nonsemantic_named`: `OpExtInstImport "NonSemantic.Foo.1"`, `%nsi = OpExtInst %void %foo 1`,
  `OpName %nsi "nsi"` (crash).
- `E_glsl_extinst_named`: `OpName` on a GLSL.std.450 `UMin` result (OK).
- `F_no_debuginfo`: the base shader (OK).
- `slang_example.slang`, `slang_example.g2.spvasm` / `.spv`: real-world case, `slangc -g2` output
  (crash; only the file path and command-line `OpString`s replaced with neutral text);
  `slang_example.g2.no_opname.spvasm` / `.spv`: the same module with only `OpName %35 "id"`
  deleted (OK).

## Minimal module (`A_debugsource_named.spvasm`)

```
               OpCapability Shader
               OpExtension "SPV_KHR_non_semantic_info"
        %dbg = OpExtInstImport "NonSemantic.Shader.DebugInfo.100"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main" %buf %gid
               OpExecutionMode %main LocalSize 64 1 1
       %file = OpString "a.comp"
      %uname = OpString "uint"
               OpName %main "main"
               OpName %src "src"                         ; <-- remove this line and it works
               OpDecorate %arr ArrayStride 4
               OpDecorate %Buf Block
               OpMemberDecorate %Buf 0 Offset 0
               OpDecorate %buf Binding 0
               OpDecorate %buf DescriptorSet 0
               OpDecorate %gid BuiltIn GlobalInvocationId
       %void = OpTypeVoid
         %fn = OpTypeFunction %void
       %uint = OpTypeInt 32 0
        %arr = OpTypeRuntimeArray %uint
        %Buf = OpTypeStruct %arr
      %p_Buf = OpTypePointer StorageBuffer %Buf
        %buf = OpVariable %p_Buf StorageBuffer
     %v3uint = OpTypeVector %uint 3
     %p_gid = OpTypePointer Input %v3uint
        %gid = OpVariable %p_gid Input
     %uint_0 = OpConstant %uint 0
    %uint_32 = OpConstant %uint 32
     %uint_6 = OpConstant %uint 6
     %p_uint = OpTypePointer Input %uint
    %ps_uint = OpTypePointer StorageBuffer %uint
        %src = OpExtInst %void %dbg DebugSource %file
       %main = OpFunction %void None %fn
      %entry = OpLabel
         %gp = OpAccessChain %p_uint %gid %uint_0
          %x = OpLoad %uint %gp
         %dp = OpAccessChain %ps_uint %buf %uint_0 %x
               OpStore %dp %x
               OpReturn
               OpFunctionEnd
```

Slang source for the real-world case (`slangc slang_example.slang -target spirv -profile spirv_1_6
-entry main -g2 -o slang_example.g2.spv`, slangc 2026.17 from Vulkan SDK 1.4.363.0):

```hlsl
RWStructuredBuffer<uint> data;
[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    data[id.x] = id.x;
}
```

Its output contains `OpName %35 "id"` with `%35 = OpExtInst %void %2 DebugLocalVariable ...`.

## Steps

```
cl /nologo /W3 vk_pipe.c /I"%VULKAN_SDK%\Include" /link /LIBPATH:"%VULKAN_SDK%\Lib" vulkan-1.lib
spirv-as --target-env vulkan1.3 A_debugsource_named.spvasm -o A_debugsource_named.spv
vk_pipe.exe A_debugsource_named.spv AMD
vk_pipe.exe B_debugsource_unnamed.spv AMD
```

## Expected

`OK` (pipeline created) for every file.

## Actual

```
> vk_pipe.exe A_debugsource_named.spv AMD
* AMD Radeon RX 6900 XT | AMD proprietary driver | 26.10.1 (AMD proprietary shader compiler) | driverVersion 0x800161
  Intel(R) UHD Graphics | Intel Corporation | 101.7084 | driverVersion 0x195bac
vkCreateComputePipelines...
```

Exit code 0xC0000005. Windows Application log (event 1000): faulting module `amdvlk64.dll`
32.0.21045.16010, exception code 0xc0000005, fault offset 0x00000000022ea3ba (same offset for the
minimal module and for the Slang module).

## Verified

On the machine above (2026-10-25), AMD RX 6900 XT vs. Intel UHD Graphics 101.7084 in the same box:

| Module | AMD | Intel |
|---|---|---|
| A: `DebugSource` + `OpName` on it | crash | OK |
| B: `DebugSource`, no `OpName` | OK | OK |
| C: `DebugTypeBasic` + `OpName` on it | crash | OK |
| D: `NonSemantic.Foo.1` instruction + `OpName` on it | crash | OK |
| E: `OpName` on a GLSL.std.450 result | OK | OK |
| F: no debug info | OK | OK |
| Slang `-g2` | crash | OK |
| Slang `-g2`, `OpName %35` deleted | OK | not run |
| Slang `-g0`, `-g1` | OK | OK |
| glslang `-g` (OpLine), `-gV`, `-gVS` (DebugInfo.100 and .102, incl. local variables, `DebugDeclare`, `DebugValue`, function parameters) | OK | OK |

All of A..F also work on Linux with RADV (Mesa 26.2.5).

Not checked: other AMD GPUs or drivers, graphics pipelines, `OpDecorate` on non-semantic results.

## Workaround

Strip `OpName` instructions whose target is a non-semantic `OpExtInst` before handing the module to
the driver (or use Slang `-g1`/glslang for debug builds).

---

## Appendix: possible comment for shader-slang/slang#8051 (closed)

> Root cause on the AMD side, found with a minimal module: amdvlk64.dll (32.0.21045, AMD Software
> 26.10.1, RX 6900 XT) crashes in pipeline creation whenever an `OpName` targets the result of a
> `NonSemantic.*` `OpExtInst`. Slang `-g2` emits `OpName %x "<var>"` on every `DebugLocalVariable`;
> deleting just that `OpName` from the `slangc -g2` output makes the pipeline compile. glslang's
> DebugInfo output (no `OpName` on debug instructions) works. Given SPV_KHR_non_semantic_info's
> "Result IDs from these non-semantic instruction set instructions must only be be used in other
> non-semantic instructions", Slang might want to stop naming debug instructions (the name is
> already in the `DebugLocalVariable` operands); reported to AMD as well.
