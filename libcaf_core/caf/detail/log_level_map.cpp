#include "caf/detail/log_level_map.hpp"

#include "caf/config.hpp"
#include "caf/detail/log_level.hpp"
#include "caf/string_algorithms.hpp"

#include <algorithm>

namespace caf::detail {

namespace {

/// The predicate for searching the log level map. This models `!(a < b)`, i.e.,
/// the elements are sorted in *descending* order.
struct predicate {
  using kvp_t = std::pair<unsigned, std::string>;

  bool operator()(unsigned lhs, const kvp_t& rhs) const noexcept {
    return lhs > rhs.first;
  }

  bool operator()(const kvp_t& lhs, unsigned rhs) const noexcept {
    return lhs.first > rhs;
  }
};

} // namespace

log_level_map::log_level_map() {
  // Elements are sorted in descending order for efficient lookup.
  mapping_.reserve(8);
  mapping_.emplace_back(CAF_LOG_LEVEL_TRACE, "TRACE");
  mapping_.emplace_back(CAF_LOG_LEVEL_DEBUG, "DEBUG");
  mapping_.emplace_back(CAF_LOG_LEVEL_INFO, "INFO");
  mapping_.emplace_back(CAF_LOG_LEVEL_WARNING, "WARNING");
  mapping_.emplace_back(CAF_LOG_LEVEL_ERROR, "ERROR");
}

std::string_view log_level_map::operator[](unsigned level) const noexcept {
  for (auto i = mapping_.begin(); i != mapping_.end(); ++i)
    if (level >= i->first)
      return i->second;
  return "OFF";
}

unsigned log_level_map::by_name(std::string_view val) const noexcept {
  auto& xs = mapping_;
  auto i = std::find_if(xs.begin(), xs.end(), [val](auto& kvp) {
    return icase_equal(kvp.second, val);
  });
  if (i != xs.end())
    return i->first;
  return 0;
}

void log_level_map::set(std::string name, unsigned level) {
  predicate f;
  auto i = std::lower_bound(mapping_.begin(), mapping_.end(), level, f);
  if (i != mapping_.end() && i->first == level)
    i->second = std::move(name);
  else
    mapping_.emplace(i, level, std::move(name));
}

} // namespace caf::detail
