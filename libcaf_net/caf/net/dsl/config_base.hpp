// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/callback.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/net/dsl/base.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/ref_counted.hpp"
#include "caf/uri.hpp"

#include <cassert>
#include <cstdint>
#include <string>

namespace caf::net::dsl {

/// Base class for configuration objects.
class CAF_NET_EXPORT config_base : public ref_counted {
public:
  explicit config_base(multiplexer* mpx) : mpx(mpx) {
    // nop
  }

  config_base(const config_base&) = default;

  config_base& operator=(const config_base&) = default;

  virtual ~config_base();

  /// The pointer to the parent @ref multiplexer.
  multiplexer* mpx;

  /// User-defined callback for errors.
  shared_callback_ptr<void(const error&)> on_error;

  /// Calls `on_error` if non-null.
  void call_on_error(const error& what) {
    if (on_error)
      (*on_error)(what);
  }
};

/// Simple base class for a configuration with a trait member.
template <class Trait>
class config_with_trait : public config_base {
public:
  using trait_type = Trait;

  explicit config_with_trait(multiplexer* mpx, Trait trait)
    : config_base(mpx), trait(std::move(trait)) {
    // nop
  }

  config_with_trait(const config_with_trait&) = default;

  config_with_trait& operator=(const config_with_trait&) = default;

  /// Configures various aspects of the protocol stack such as in- and output
  /// types.
  Trait trait;
};

} // namespace caf::net::dsl
