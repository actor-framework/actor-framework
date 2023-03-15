// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/intrusive_ptr.hpp"
#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/config_base.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/context.hpp"

#include <string_view>

namespace caf::net::dsl {

/// Meta programming utility.
template <class T>
struct generic_config_tag {
  using type = T;
};

/// Wraps configuration of some base parameters before we know whether the user
/// is starting a client or a server.
class generic_config {
public:
  /// Configuration for a client that creates the socket on demand.
  class lazy : public has_ctx {
  public:
    static constexpr std::string_view name = "lazy";
  };

  static constexpr auto lazy_v = generic_config_tag<lazy>{};

  static constexpr auto fail_v = generic_config_tag<error>{};

  template <class Base>
  class value : public config_impl<Base, lazy> {
  public:
    using super = config_impl<Base, lazy>;

    using super::super;

    static auto make(multiplexer* mpx) {
      return make_counted<value>(mpx, std::in_place_type<lazy>);
    }
  };
};

template <class Base>
using generic_config_value = typename generic_config::value<Base>;

template <class Base>
using generic_config_ptr = intrusive_ptr<generic_config_value<Base>>;

} // namespace caf::net::dsl
