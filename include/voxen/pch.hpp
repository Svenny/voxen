#pragma once

// Extras are not expected to change frequently, but they contain a fair
// amount of template-based code, so it might be wise to precompile them
#include <extras/bitset.hpp>
#include <extras/defer.hpp>
#include <extras/dyn_array.hpp>
#include <extras/enum_utils.hpp>
#include <extras/fixed_pool.hpp>
#include <extras/function_ref.hpp>
#include <extras/hardware_params.hpp>
#include <extras/linear_allocator.hpp>
#include <extras/math.hpp>
#include <extras/pimpl.hpp>
#include <extras/refcnt_ptr.hpp>
#include <extras/source_location.hpp>
#include <extras/string_utils.hpp>

#include <cpp/result.hpp>

// `glm.hpp` and `ext.hpp` should include all non-experimental GLM
#include <glm/ext.hpp>
#include <glm/glm.hpp>

// Not sure if there are headers not transitively included by these
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/compile.h>
#include <fmt/format.h>
#include <fmt/std.h>

// Hope all std headers are listed here :)
#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <barrier>
#include <bit>
#include <bitset>
#include <cassert>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <clocale>
#include <cmath>
#include <compare>
#include <complex>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <exception>
#include <filesystem>
#include <forward_list>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <latch>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <numbers>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <ranges>
#include <ratio>
#include <regex>
#include <semaphore>
#include <set>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <valarray>
#include <variant>
#include <vector>
#include <version>
