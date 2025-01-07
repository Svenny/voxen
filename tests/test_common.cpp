#include "test_common.hpp"

#include <fmt/format.h>

namespace Catch
{

std::string StringMaker<std::error_code>::convert(const std::error_code& ec)
{
	return fmt::format("[{}:{}] ({})", ec.category().name(), ec.value(), ec.message());
}

std::string StringMaker<std::error_condition>::convert(const std::error_condition& ec)
{
	return fmt::format("[{}:{}] ({})", ec.category().name(), ec.value(), ec.message());
}

#define MAKE_GLM_VEC_STRINGIZER(L, T, ...) \
	std::string StringMaker<glm::vec<L, T>>::convert(const glm::vec<L, T>& value) \
	{ \
		return fmt::format(__VA_ARGS__); \
	}

MAKE_GLM_VEC_STRINGIZER(2, float, "vec2({}, {})", value.x, value.y);
MAKE_GLM_VEC_STRINGIZER(3, float, "vec3({}, {}, {})", value.x, value.y, value.z);
MAKE_GLM_VEC_STRINGIZER(4, float, "vec4({}, {}, {}, {})", value.x, value.y, value.z, value.w);

MAKE_GLM_VEC_STRINGIZER(2, double, "dvec2({}, {})", value.x, value.y);
MAKE_GLM_VEC_STRINGIZER(3, double, "dvec3({}, {}, {})", value.x, value.y, value.z);
MAKE_GLM_VEC_STRINGIZER(4, double, "dvec4({}, {}, {}, {})", value.x, value.y, value.z, value.w);

MAKE_GLM_VEC_STRINGIZER(2, int32_t, "ivec2({}, {})", value.x, value.y);
MAKE_GLM_VEC_STRINGIZER(3, int32_t, "ivec3({}, {}, {})", value.x, value.y, value.z);
MAKE_GLM_VEC_STRINGIZER(4, int32_t, "ivec4({}, {}, {}, {})", value.x, value.y, value.z, value.w);

MAKE_GLM_VEC_STRINGIZER(2, uint32_t, "uvec2({}, {})", value.x, value.y);
MAKE_GLM_VEC_STRINGIZER(3, uint32_t, "uvec3({}, {}, {})", value.x, value.y, value.z);
MAKE_GLM_VEC_STRINGIZER(4, uint32_t, "uvec4({}, {}, {}, {})", value.x, value.y, value.z, value.w);

MAKE_GLM_VEC_STRINGIZER(2, bool, "bvec2({}, {})", value.x, value.y);
MAKE_GLM_VEC_STRINGIZER(3, bool, "bvec3({}, {}, {})", value.x, value.y, value.z);
MAKE_GLM_VEC_STRINGIZER(4, bool, "bvec4({}, {}, {}, {})", value.x, value.y, value.z, value.w);

} // namespace Catch
