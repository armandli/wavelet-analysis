#ifndef UTIL_ALIASES_H
#define UTIL_ALIASES_H

#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wa {

namespace s = std;
namespace sr = std::ranges;
namespace sv = std::views;

using str  = s::string;
using strv = s::string_view;

template <typename T, typename A = s::allocator<T>>
using vec = s::vector<T, A>;

template <typename T>
using opt = s::optional<T>;

template <typename T, typename D = s::default_delete<T>>
using uptr = s::unique_ptr<T, D>;

template <typename T>
using sptr = s::shared_ptr<T>;

template <typename... Ts>
using umap = s::unordered_map<Ts...>;

template <typename... Ts>
using uset = s::unordered_set<Ts...>;

}  // namespace wa

#endif  // UTIL_ALIASES_H
