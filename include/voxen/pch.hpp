#pragma once

// Extras are not expected to change frequently, but they contain a fair
// amount of template-based code, so it might be wise to precompile them
#include <extras/bitset.hpp>
#include <extras/defer.hpp>
#include <extras/dyn_array.hpp>
#include <extras/enum_utils.hpp>
#include <extras/fixed_pool.hpp>
#include <extras/function_ref.hpp>
#include <extras/math.hpp>
#include <extras/pimpl.hpp>
#include <extras/refcnt_ptr.hpp>
#include <extras/source_location.hpp>
#include <extras/spinlock.hpp>
#include <extras/string_utils.hpp>

// `glm.hpp` and `ext.hpp` should include all non-experimental GLM
#include <glm/glm.hpp>
#include <glm/ext.hpp>

// Not sure if there are headers not transitively included by these
#include <fmt/compile.h>
#include <fmt/format.h>

// Hope all std headers are listed here :)
#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <clocale>
#include <cmath>
#include <complex>
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
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <ratio>
#include <regex>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <stack>
#include <stdexcept>
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
