// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/consumer.hpp"
#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"

namespace caf::net {

/// Connects a socket manager to an asynchronous consumer resource. Whenever new
/// data becomes ready, the adapter registers the socket manager for writing.
template <class Buffer>
class consumer_adapter final : public detail::atomic_ref_counted,
                               public async::consumer {
public:
  using buf_ptr = intrusive_ptr<Buffer>;

  using ptr_type = intrusive_ptr<consumer_adapter>;

  void on_producer_ready() override {
    // nop
  }

  void on_producer_wakeup() override {
    mgr_->schedule(do_wakeup_);
  }

  void ref_consumer() const noexcept override {
    this->ref();
  }

  void deref_consumer() const noexcept override {
    this->deref();
  }

  template <class Policy, class Observer>
  std::pair<bool, size_t> pull(Policy policy, size_t demand, Observer& dst) {
    return buf_->pull(policy, demand, dst);
  }

  void cancel() {
    buf_->cancel();
    buf_ = nullptr;
    mgr_ = nullptr;
    do_wakeup_.dispose();
    do_wakeup_ = nullptr;
  }

  bool has_data() const noexcept {
    return buf_->has_data();
  }

  static ptr_type make(buf_ptr buf, socket_manager_ptr mgr, action do_wakeup) {
    if (buf) {
      auto adapter = ptr_type{new consumer_adapter(buf, mgr,
                                                   std::move(do_wakeup)), //
                              false};
      buf->set_consumer(adapter);
      return adapter;
    } else {
      return nullptr;
    }
  }

private:
  consumer_adapter(buf_ptr buf, socket_manager_ptr mgr, action do_wakeup)
    : buf_(std::move(buf)),
      mgr_(std::move(mgr)),
      do_wakeup_(std::move(do_wakeup)) {
    // nop
  }

  intrusive_ptr<Buffer> buf_;
  socket_manager_ptr mgr_;
  action do_wakeup_;
  action do_cancel_;
};

template <class T>
using consumer_adapter_ptr = intrusive_ptr<consumer_adapter<T>>;

} // namespace caf::net
