#include <voxen/debug/bug_found.hpp>

#include <voxen/util/log.hpp>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <stacktrace>
#include <string>
#include <vector>

#ifndef _WIN32
	#include <climits>
	#include <cstdlib>
	#include <link.h>
#else
	#include <Windows.h>
#endif

namespace voxen::debug
{

namespace
{

// Binary (executable or shared library) loaded into the process.
// Frames are printed as offsets in modules, symbolizers take these offsets
// and debug info matching `debug_id` to recover what was there:
//     Linux: `llvm-symbolizer --obj=<binary> <offset>`
//     Windows: `llvm-symbolizer --relative-address --obj=<binary> <offset>`
struct Module {
	std::string path;
	uintptr_t base = 0;
	// Build ID on Linux, PDB signature (GUID and age, as symbol servers know it) on Windows
	std::string debug_id;

	std::string_view name() const noexcept
	{
		size_t slash = path.find_last_of("/\\");
		return slash == std::string::npos ? std::string_view(path) : std::string_view(path).substr(slash + 1);
	}
};

#ifndef _WIN32
std::string toHex(std::span<const uint8_t> bytes)
{
	std::string result;
	for (uint8_t byte : bytes) {
		result += fmt::format("{:02x}", byte);
	}
	return result;
}

// Searches for the GNU build ID note in a note segment, assuming
// note fields are padded to `align` bytes in absolute addresses
std::string findBuildIdNote(const uint8_t *pos, const uint8_t *end, uintptr_t align)
{
	auto align_up = [align](const uint8_t *ptr) {
		return reinterpret_cast<const uint8_t *>((reinterpret_cast<uintptr_t>(ptr) + align - 1) & ~(align - 1));
	};

	while (pos + sizeof(ElfW(Nhdr)) <= end) {
		ElfW(Nhdr) note;
		memcpy(&note, pos, sizeof(note));

		const uint8_t *name = pos + sizeof(note);
		const uint8_t *desc = align_up(name + note.n_namesz);
		pos = align_up(desc + note.n_descsz);

		if (note.n_type == NT_GNU_BUILD_ID && note.n_namesz == 4 && pos <= end && memcmp(name, "GNU", 4) == 0) {
			return toHex(std::span(desc, note.n_descsz));
		}
	}

	return {};
}

std::string findBuildId(const dl_phdr_info &info)
{
	for (const ElfW(Phdr) &phdr : std::span(info.dlpi_phdr, info.dlpi_phnum)) {
		if (phdr.p_type != PT_NOTE) {
			continue;
		}

		auto *begin = reinterpret_cast<const uint8_t *>(info.dlpi_addr + phdr.p_vaddr);
		const uint8_t *end = begin + phdr.p_memsz;

		// Note segments are aligned to 4 or 8 bytes, and notes in them should be too. But linkers
		// may put 4-aligned notes (like build ID) into an 8-aligned segment, retry with 4 then.
		std::string build_id = findBuildIdNote(begin, end, phdr.p_align == 8 ? 8 : 4);
		if (build_id.empty() && phdr.p_align == 8) {
			build_id = findBuildIdNote(begin, end, 4);
		}

		if (!build_id.empty()) {
			return build_id;
		}
	}

	return {};
}

std::optional<Module> findModule(uintptr_t pc)
{
	struct Search {
		uintptr_t pc;
		std::optional<Module> result;
	} search { .pc = pc, .result = std::nullopt };

	dl_iterate_phdr(
		[](dl_phdr_info *info, size_t, void *data) {
			auto &s = *static_cast<Search *>(data);

			for (const ElfW(Phdr) &phdr : std::span(info->dlpi_phdr, info->dlpi_phnum)) {
				uintptr_t begin = info->dlpi_addr + phdr.p_vaddr;
				if (phdr.p_type != PT_LOAD || s.pc < begin || s.pc - begin >= phdr.p_memsz) {
					continue;
				}

				std::string path = info->dlpi_name;
				if (path.empty()) {
					// The main executable has no name here
					char buf[PATH_MAX];
					path = realpath("/proc/self/exe", buf) ? buf : "<executable>";
				}

				// `dlpi_addr` is the load bias, `pc - bias` is the address in ELF file terms
				s.result = Module { .path = std::move(path), .base = info->dlpi_addr, .debug_id = findBuildId(*info) };
				return 1;
			}

			return 0;
		},
		&search);

	return search.result;
}
#else
std::string findPdbSignature(uintptr_t base)
{
	auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
	auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + static_cast<uintptr_t>(dos->e_lfanew));
	const IMAGE_DATA_DIRECTORY &dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];

	auto *entries = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY *>(base + dir.VirtualAddress);
	for (const IMAGE_DEBUG_DIRECTORY &entry : std::span(entries, dir.Size / sizeof(IMAGE_DEBUG_DIRECTORY))) {
		// CodeView PDB 7.0 info: "RSDS", GUID, age, PDB path
		struct CvInfoPdb70 {
			DWORD signature;
			GUID guid;
			DWORD age;
			char pdb_path[1];
		};

		if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW || entry.AddressOfRawData == 0
			|| entry.SizeOfData < sizeof(CvInfoPdb70)) {
			continue;
		}

		auto *cv = reinterpret_cast<const CvInfoPdb70 *>(base + entry.AddressOfRawData);
		if (cv->signature != 0x53445352) {
			continue;
		}

		// Symbol servers key PDBs by this, uppercase GUID bytes followed by age
		const GUID &g = cv->guid;
		std::string signature = fmt::format("{:08X}{:04X}{:04X}", g.Data1, g.Data2, g.Data3);
		for (uint8_t byte : g.Data4) {
			signature += fmt::format("{:02X}", byte);
		}

		return fmt::format("{}{:X} ({})", signature, cv->age, cv->pdb_path);
	}

	return {};
}

std::optional<Module> findModule(uintptr_t pc)
{
	HMODULE module = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			 reinterpret_cast<LPCWSTR>(pc), &module)) {
		return std::nullopt;
	}

	wchar_t wpath[MAX_PATH];
	DWORD wlen = GetModuleFileNameW(module, wpath, MAX_PATH);
	char path[MAX_PATH * 3];
	int len = WideCharToMultiByte(CP_UTF8, 0, wpath, static_cast<int>(wlen), path, sizeof(path), nullptr, nullptr);

	auto base = reinterpret_cast<uintptr_t>(module);
	return Module { .path = std::string(path, static_cast<size_t>(std::max(len, 0))),
		.base = base,
		.debug_id = findPdbSignature(base) };
}
#endif

} // namespace

void bugFound(std::string_view message, extras::source_location where)
{
	Log::fatal("----[ BUG FOUND ]----", where);
	Log::fatal("Please fill an issue on https://github.com/Svenny/voxen", where);
	Log::fatal("and attach this log output. Some related information:", where);
	Log::fatal("Explanation message: {}", message, where);

	// Skip the frame of this function
	auto trace = std::stacktrace::current(1);
	Log::fatal("Stack trace:", where);

	std::vector<Module> modules;

	for (size_t i = 0; const std::stacktrace_entry &entry : trace) {
		if (!entry) {
			// libstdc++ can add an invalid terminating entry
			continue;
		}

		// Native handle is integer in libstdc++ but pointer in MSVC STL.
		// libstdc++ points it into the call instruction already, MSVC STL gives
		// the return address. Step back into the call to get the right line.
		auto pc = std::bit_cast<uintptr_t>(entry.native_handle());
#ifdef _WIN32
		pc--;
#endif

		std::string location = fmt::format("{:#x}", pc);
		std::optional<Module> module = findModule(pc);
		if (module) {
			location = fmt::format("{}+{:#x}", module->name(), pc - module->base);
			if (std::ranges::find(modules, module->base, &Module::base) == modules.end()) {
				modules.emplace_back(std::move(*module));
			}
		}

		if (entry.source_file().empty()) {
			Log::fatal("#{} {} {}", i, location, entry.description(), where);
		} else {
			Log::fatal("#{} {} {} at {}:{}", i, location, entry.description(), entry.source_file(), entry.source_line(),
				where);
		}

		i++;
	}

	Log::fatal("Modules:", where);
	for (const Module &module : modules) {
		Log::fatal("{}: {}, debug ID {}", module.name(), module.path,
			module.debug_id.empty() ? "unknown" : module.debug_id, where);
	}

	// TODO: save crash dump
	// TODO: initiate emergency game save?

	Log::fatal("----[ ABORTING VOXEN ]----");

	// Try to break into debugger if it's present
	// TODO: use debugger detection mechanism, otherwise save crash dump and abort
	__builtin_debugtrap();
	abort();
}

} // namespace voxen::debug
