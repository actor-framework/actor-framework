// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config_option.hpp"
#include "caf/config_value.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/fwd.hpp"
#include "caf/pec.hpp"

#include <memory>
#include <string_view>

namespace caf::detail {

template <class T>
error sync_impl(void* ptr, config_value& x) {
  if (auto val = get_as<T>(x)) {
    if (auto err = x.assign(*val); !err) {
      if (ptr)
        *static_cast<T*>(ptr) = std::move(*val);
      return none;
    } else {
      return err;
    }
  } else {
    return std::move(val.error());
  }
}

template <class T>
config_value get_impl(const void* ptr) {
  config_value result;
  auto err = result.assign(*static_cast<const T*>(ptr));
  static_cast<void>(err); // Safe to discard because sync() fails otherwise.
  return result;
}

template <class T>
config_option::meta_state* option_meta_state_instance() {
  static config_option::meta_state obj{sync_impl<T>, get_impl<T>,
                                       config_value::mapped_type_name<T>()};
  return &obj;
}

} // namespace caf::detail

namespace caf {

/// Creates a config option that synchronizes with `storage`.
template <class T>
config_option make_config_option(std::string_view category,
                                 std::string_view name,
                                 std::string_view description) {
  return {category, name, description, detail::option_meta_state_instance<T>()};
}

/// Creates a config option that synchronizes with `storage`.
template <class T>
config_option make_config_option(T& storage, std::string_view category,
                                 std::string_view name,
                                 std::string_view description) {
  return {category, name, description, detail::option_meta_state_instance<T>(),
          std::addressof(storage)};
}

// -- backward compatibility, do not use for new code ! ------------------------

// Inverts the value when writing to `storage`.
CAF_CORE_EXPORT config_option
make_negated_config_option(bool& storage, std::string_view category,
                           std::string_view name, std::string_view description);

} // namespace caf
