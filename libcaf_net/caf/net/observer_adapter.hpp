// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/observer_buffer.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"

namespace caf::net {

/// Connects a socket manager to an asynchronous publisher using a buffer.
/// Whenever the buffer becomes non-empty, the adapter registers the socket
/// manager for writing. The usual pattern for using the adapter then is to call
/// `poll` on the adapter in `prepare_send`.
template <class T>
class observer_adapter : public async::observer_buffer<T> {
public:
  using super = async::observer_buffer<T>;

  explicit observer_adapter(socket_manager* owner) : mgr_(owner) {
    // nop
  }

private:
  void deinit(std::unique_lock<std::mutex>& guard) final {
    wakeup(guard);
    mgr_ = nullptr;
  }

  void wakeup(std::unique_lock<std::mutex>&) final {
    mgr_->mpx().register_writing(mgr_);
  }

  intrusive_ptr<socket_manager> mgr_;
};

template <class T>
using observer_adapter_ptr = intrusive_ptr<observer_adapter<T>>;

} // namespace caf::net
