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

  config_base(const config_base&) = delete;

  config_base& operator=(const config_base&) = delete;

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

} // namespace caf::net::dsl
