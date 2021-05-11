// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/poll_subscriber.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"

namespace caf::net {

/// Base class for buffered consumption of published items.
template <class T>
class subscriber_adapter : public flow::poll_subscriber<T> {
public:
  using super = flow::poll_subscriber<T>;

  explicit subscriber_adapter(socket_manager* owner) : mgr_(owner) {
    // nop
  }

private:
  void wakeup(std::unique_lock<std::mutex>&) {
    mgr_->mpx().register_writing(mgr_);
  }

  intrusive_ptr<socket_manager> mgr_;
};

template <class T>
using subscriber_adapter_ptr = intrusive_ptr<subscriber_adapter<T>>;

} // namespace caf::net
