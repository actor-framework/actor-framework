// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/config_option_adder.hpp"

#include "caf/config.hpp"
#include "caf/config_option_set.hpp"

CAF_PUSH_DEPRECATED_WARNING

namespace caf {

config_option_adder::config_option_adder(config_option_set& target,
                                         string_view category)
    : xs_(target),
      category_(category) {
  // nop
}

config_option_adder& config_option_adder::add_neg(bool& ref, string_view name,
                                                  string_view description) {
  return add_impl(make_negated_config_option(ref, category_,
                                             name, description));
}

config_option_adder& config_option_adder::add_impl(config_option&& opt) {
  xs_.add(std::move(opt));
  return *this;
}

} // namespace caf

CAF_POP_WARNINGS
