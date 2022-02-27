// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/consumer.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/ref_counted.hpp"

namespace caf::net {

/// Connects a socket manager to an asynchronous consumer resource. Whenever new
/// data becomes ready, the adapter registers the socket manager for writing.
template <class Buffer>
class consumer_adapter final : public ref_counted, public async::consumer {
public:
  using buf_ptr = intrusive_ptr<Buffer>;

  void on_producer_ready() override {
    // nop
  }

  void on_producer_wakeup() override {
    mgr_->mpx().schedule_fn([adapter = strong_this()] { //
      adapter->on_wakeup();
    });
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
  }

  bool has_data() const noexcept {
    return buf_->has_data();
  }

  /// Tries to open the resource for reading.
  /// @returns a connected adapter that reads from the resource on success,
  ///          `nullptr` otherwise.
  template <class Resource>
  static intrusive_ptr<consumer_adapter>
  try_open(socket_manager* owner, Resource src) {
    CAF_ASSERT(owner != nullptr);
    if (auto buf = src.try_open()) {
      using ptr_type = intrusive_ptr<consumer_adapter>;
      auto adapter = ptr_type{new consumer_adapter(owner, buf), false};
      buf->set_consumer(adapter);
      return adapter;
    } else {
      return nullptr;
    }
  }

  friend void intrusive_ptr_add_ref(const consumer_adapter* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const consumer_adapter* ptr) noexcept {
    ptr->deref();
  }

private:
  consumer_adapter(socket_manager* owner, buf_ptr buf)
    : mgr_(owner), buf_(std::move(buf)) {
    // nop
  }

  auto strong_this() {
    return intrusive_ptr{this};
  }

  void on_wakeup() {
    if (buf_ && buf_->has_consumer_event()) {
      mgr_->mpx().register_writing(mgr_);
    }
  }

  intrusive_ptr<socket_manager> mgr_;
  intrusive_ptr<Buffer> buf_;
};

template <class T>
using consumer_adapter_ptr = intrusive_ptr<consumer_adapter<T>>;

} // namespace caf::net
