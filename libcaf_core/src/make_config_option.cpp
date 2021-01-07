// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/make_config_option.hpp"

#include <ctype.h>

#include "caf/config_value.hpp"
#include "caf/optional.hpp"

namespace caf {

namespace {

using meta_state = config_option::meta_state;

error bool_sync_neg(void* ptr, config_value& x) {
  if (auto val = get_as<bool>(x)) {
    x = config_value{*val};
    if (ptr)
      *static_cast<bool*>(ptr) = !*val;
    return none;
  } else {
    return std::move(val.error());
  }
}

config_value bool_get_neg(const void* ptr) {
  return config_value{!*static_cast<const bool*>(ptr)};
}

meta_state bool_neg_meta{bool_sync_neg, bool_get_neg, "bool"};

template <uint64_t Denominator>
error sync_timespan(void* ptr, config_value& x) {
  if (auto val = get_as<timespan>(x)) {
    x = config_value{*val};
    if (ptr)
      *static_cast<size_t*>(ptr) = static_cast<size_t>(get<timespan>(x).count())
                                   / Denominator;
    return none;
  } else {
    return std::move(val.error());
  }
}

template <uint64_t Denominator>
config_value get_timespan(const void* ptr) {
  auto ival = static_cast<int64_t>(*static_cast<const size_t*>(ptr));
  timespan val{ival * Denominator};
  return config_value{val};
}

meta_state us_res_meta{sync_timespan<1000>, get_timespan<1000>, "timespan"};

meta_state ms_res_meta{sync_timespan<1000000>, get_timespan<1000000>,
                       "timespan"};

} // namespace

config_option make_negated_config_option(bool& storage, string_view category,
                                         string_view name,
                                         string_view description) {
  return {category, name, description, &bool_neg_meta, &storage};
}

config_option
make_us_resolution_config_option(size_t& storage, string_view category,
                                 string_view name, string_view description) {
  return {category, name, description, &us_res_meta, &storage};
}

config_option
make_ms_resolution_config_option(size_t& storage, string_view category,
                                 string_view name, string_view description) {
  return {category, name, description, &ms_res_meta, &storage};
}

} // namespace caf
