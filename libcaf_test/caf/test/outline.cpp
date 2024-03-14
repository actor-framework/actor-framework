// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/outline.hpp"

namespace caf::test {

block_type outline::type() const noexcept {
  return block_type::outline;
}

given* outline::get_given(int id, std::string_view description,
                          const detail::source_location& loc) {
  return get_nested<given>(id, description, loc);
}

and_given* outline::get_and_given(int id, std::string_view description,
                                  const detail::source_location& loc) {
  return get_nested<and_given>(id, description, loc);
}

when* outline::get_when(int id, std::string_view description,
                        const detail::source_location& loc) {
  return get_nested<when>(id, description, loc);
}

and_when* outline::get_and_when(int id, std::string_view description,
                                const detail::source_location& loc) {
  return get_nested<and_when>(id, description, loc);
}

scope outline::commit() {
  if (!ctx_->active() || !can_run())
    return {};
  enter();
  return scope{this};
}

} // namespace caf::test
