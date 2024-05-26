// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/config_base.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/context.hpp"

#include "caf/intrusive_ptr.hpp"

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
  static constexpr auto fail_v = generic_config_tag<error>{};

  static constexpr size_t fail_index = 0;

  /// Configuration for a client that creates the socket on demand.
  class lazy : public has_make_ctx {
  public:
    static constexpr std::string_view name = "lazy";
  };

  static constexpr auto lazy_v = generic_config_tag<lazy>{};

  static constexpr size_t lazy_index = 1;

  class value : public config_impl<lazy> {
  public:
    using super = config_impl<lazy>;

    explicit value(multiplexer* mpx) : super(mpx, std::in_place_type<lazy>) {
      // nop
    }
  };
};

using generic_config_value = typename generic_config::value;

} // namespace caf::net::dsl
