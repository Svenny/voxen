#include <tk/uid.hpp>

#include <tk/rng.hpp>

#include <string>
#include <string_view>

namespace tk
{

static_assert(sizeof(UID) == 16);

// It works!
static_assert(UID("c1bf2846-ff1f9f34-a0abff03-e68abb9b") == UID(0xc1bf2846ff1f9f34U, 0xa0abff03e68abb9bU));
// Full compile-time validation, neither of this will compile:
// static_assert(UID("c1bf2846-ff1f9f34-a0abff03-#68abb9b").v0 != 0);
// static_assert(UID("c1bf2846-ff1f9f34-a0abff03@e68abb9b").v0 != 0);
// static_assert(UID("c1bf2846-ff1f9f34-a0abff03-e68").v0 != 0);
// static_assert(UID("c1bf2846-ff1f9f34-a0abff03-e68abb9b###").v0 != 0);
// static_assert(UID("c1bf2846-ff1f9f34-a0abff03-e68ABB9b").v0 != 0);

std::expected<UID, std::string> UID::fromChars(std::string_view in)
{
	if (std::optional<UID> uid = tryParse(in); uid) {
		return *uid;
	}

	return std::unexpected("invalid UID '" + std::string(in)
		+ "', expected format is '########-########-########-########' with lowercase hex digits");
}

UID UID::generateRandom()
{
	return UID(rng::entropySeed(), rng::entropySeed());
}

} // namespace tk
