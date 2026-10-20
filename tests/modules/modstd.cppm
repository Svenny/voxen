// EXPERIMENT: a module using `import std;` instead of textual std includes in its global
// fragment, and importing a module (`tk.rng`) built with textual std includes

export module modstd;

import std;
import tk.rng;

export namespace modstd
{

std::string joined(std::span<const std::string> words, char separator)
{
	std::string result;
	for (const std::string &word : words) {
		if (!result.empty()) {
			result += separator;
		}
		result += word;
	}
	return result;
}

std::uint64_t firstRandom(std::uint64_t seed) noexcept
{
	return tk::rng::SplitMix64(seed)();
}

} // namespace modstd
