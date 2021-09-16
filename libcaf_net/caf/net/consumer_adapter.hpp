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
class consumer_adapter : public async::consumer, public ref_counted {
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

  template <class Policy, class OnNext, class OnError = unit_t>
  bool consume(Policy policy, size_t demand, OnNext&& on_next,
               OnError on_error = OnError{}) {
    return buf_->consume(policy, demand, std::forward<OnNext>(on_next),
                         std::move(on_error));
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
    if (has_data())
      mgr_->mpx().register_writing(mgr_);
  }

  intrusive_ptr<socket_manager> mgr_;
  intrusive_ptr<Buffer> buf_;
};

template <class T>
using consumer_adapter_ptr = intrusive_ptr<consumer_adapter<T>>;

} // namespace caf::net
