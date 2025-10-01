// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/behavior.hpp"
#include "caf/config.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/disposable.hpp"
#include "caf/log/core.hpp"
#include "caf/policy/select_any_tag.hpp"
#include "caf/sec.hpp"
#include "caf/type_list.hpp"

#include <cstddef>
#include <memory>

namespace caf::detail {

template <class F, class = typename get_callable_trait<F>::arg_types>
struct select_any_factory;

template <class F, class... Ts>
struct select_any_factory<F, type_list<Ts...>> {
  template <class Fun>
  static auto
  make(std::shared_ptr<size_t> pending, disposable timeouts, Fun f) {
    return [pending{std::move(pending)}, timeouts{std::move(timeouts)},
            f{std::move(f)}](Ts... xs) mutable {
      auto lg = log::core::trace("pending = {}", *pending);
      if (*pending > 0) {
        timeouts.dispose();
        f(xs...);
        *pending = 0;
      }
    };
  }
};

} // namespace caf::detail

namespace caf::policy {

/// Enables a `response_handle` to pick the first arriving response, ignoring
/// all other results.
/// @relates response_handle
template <class ResponseType>
class [[deprecated("obsolete, use the mail API instead")]] select_any {
public:
  static constexpr bool is_trivial = false;

  using tag_type = select_any_tag_t;
};

} // namespace caf::policy
