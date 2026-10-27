# mold: one PT_NOTE (p_align 8) holding 8-aligned and 4-aligned notes: already covered by open PR rui314/mold#1740

**No new report.** https://github.com/rui314/mold/pull/1740 ("Fix the eu-elflint failures on mold's
output", open, 2026-10-15) describes exactly this: mold merges adjacent notes into one PT_NOTE and
gives it the largest alignment, so a segment with `.note.gnu.property` (8) followed by
`.note.ABI-tag` / `.note.gnu.build-id` (4) claims 8-byte note padding. The PR changes the segment
to the smallest alignment of its notes. GNU ld and lld split note segments by alignment instead.
No separate mold issue found.

Checked locally (2026-10-25), mold 2.40.4, GNU ld 2.46, ld.lld 21.1.8, gcc 15.2, x86_64:

```
$ printf 'int main() { return 0; }\n' > m.c
$ gcc -O1 -fcf-protection=full -Wl,--build-id -fuse-ld=mold m.c -o m_mold
$ readelf -lW m_mold | grep NOTE
  NOTE           0x000300 0x0000000000000300 0x0000000000000300 0x000074 0x000074 R   0x8
$ readelf -SW m_mold | grep note
  [ 2] .note.gnu.property NOTE  0000000000000300 000300 000030 00   A  0   0  8
  [ 3] .note.ABI-tag      NOTE  0000000000000330 000330 000020 00   A  0   0  4
  [ 4] .note.gnu.build-id NOTE  0000000000000350 000350 000024 00   A  0   0  4
```

GNU ld: three PT_NOTE segments (align 4 / 8 / 4); lld: two (one align 4 with ABI-tag and
build-id, one align 8 with the property note).

Readers that go by the segment (no section headers, as for core dumps or `dl_iterate_phdr`-based
build-ID lookups) can't find the build ID:

```
$ objcopy --strip-section-headers m_mold m_mold_nosh
$ readelf -nW m_mold_nosh
Displaying notes found at file offset 0x00000300 with length 0x00000074:
  Owner                Data size 	Description
  GNU                  0x00000020	NT_GNU_PROPERTY_TYPE_0	      Properties: x86 feature: IBT, SHSTK, x86 ISA needed: x86-64-baseline
  GNU                  0x00000010	NT_GNU_ABI_TAG (ABI version tag)	    OS: Linux, ABI: 3.2.0
readelf: Warning: note with invalid namesz and/or descsz found at offset 0x50
readelf: Warning:  type: 0x3, namesize: 0x4, descsize: 0x14, alignment: 8
$ llvm-readelf -n m_mold_nosh
...
llvm-readelf: warning: 'm_mold_nosh': unable to read notes from the PT_NOTE segment with index 2: ELF note overflows container
```

(The 20-byte build-ID descriptor would need 4 bytes of padding under 8-byte alignment, which the
segment doesn't have.)

## Possible comment for PR #1740

> Another data point for this change: with mold 2.40.4, `gcc -fcf-protection=full -Wl,--build-id
> -fuse-ld=mold` puts `.note.gnu.property`, `.note.ABI-tag` and `.note.gnu.build-id` into one
> PT_NOTE with p_align 8. With section headers stripped (`objcopy --strip-section-headers`),
> both `readelf -n` ("note with invalid namesz and/or descsz ... type: 0x3, ... descsize: 0x14,
> alignment: 8") and `llvm-readelf -n` ("ELF note overflows container") lose the build ID, so
> anything that locates the build ID through program headers is affected, not just eu-elflint.
