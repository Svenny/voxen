#pragma once

#define CATCH_CONFIG_EXPERIMENTAL_REDIRECT
#include <catch2/catch.hpp>

#include <glm/glm.hpp>

#include <system_error>

namespace Catch
{

template<>
struct StringMaker<std::error_code> {
	static std::string convert(const std::error_code &ec);
};

template<>
struct StringMaker<std::error_condition> {
	static std::string convert(const std::error_condition &ec);
};

#define MAKE_GLM_VEC_STRINGIZER(L, T) \
	template<> \
	struct StringMaker<glm::vec<L, T>> { \
		static std::string convert(const glm::vec<L, T> &value); \
	}

MAKE_GLM_VEC_STRINGIZER(2, float);
MAKE_GLM_VEC_STRINGIZER(3, float);
MAKE_GLM_VEC_STRINGIZER(4, float);

MAKE_GLM_VEC_STRINGIZER(2, double);
MAKE_GLM_VEC_STRINGIZER(3, double);
MAKE_GLM_VEC_STRINGIZER(4, double);

MAKE_GLM_VEC_STRINGIZER(2, int32_t);
MAKE_GLM_VEC_STRINGIZER(3, int32_t);
MAKE_GLM_VEC_STRINGIZER(4, int32_t);

MAKE_GLM_VEC_STRINGIZER(2, uint32_t);
MAKE_GLM_VEC_STRINGIZER(3, uint32_t);
MAKE_GLM_VEC_STRINGIZER(4, uint32_t);

MAKE_GLM_VEC_STRINGIZER(2, bool);
MAKE_GLM_VEC_STRINGIZER(3, bool);
MAKE_GLM_VEC_STRINGIZER(4, bool);

#undef MAKE_GLM_VEC_STRINGIZER

} // namespace Catch
