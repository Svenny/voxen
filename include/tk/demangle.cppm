module;

#include <optional>
#include <string>
#include <string_view>

export module tk.demangle;

export namespace tk
{

// Demangle a symbol name mangled per the Itanium C++ ABI (`_Z...`), used by Clang and GCC
// everywhere except Windows. Based on LLVM's demangler, which understands everything Clang
// emits, unlike libstdc++'s `abi::__cxa_demangle` (e.g. C++20 constrained template parameters).
// Returns `std::nullopt` if `name` is not a valid mangled symbol name.
std::optional<std::string> demangle(std::string_view name);

} // namespace tk
