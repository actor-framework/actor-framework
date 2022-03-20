// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <memory>
#include <new>

#include "caf/async/producer.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/ref_counted.hpp"

namespace caf::net {

/// Connects a socket manager to an asynchronous producer resource.
template <class Buffer>
class producer_adapter final : public ref_counted, public async::producer {
public:
  using atomic_count = std::atomic<size_t>;

  using buf_ptr = intrusive_ptr<Buffer>;

  using value_type = typename Buffer::value_type;

  void on_consumer_ready() override {
    // nop
  }

  void on_consumer_cancel() override {
    mgr_->mpx().schedule_fn([adapter = strong_this()] { //
      adapter->on_cancel();
    });
  }

  void on_consumer_demand(size_t new_demand) override {
    mgr_->continue_reading();
  }

  void ref_producer() const noexcept override {
    this->ref();
  }

  void deref_producer() const noexcept override {
    this->deref();
  }

  /// Tries to open the resource for writing.
  /// @returns a connected adapter that writes to the resource on success,
  ///          `nullptr` otherwise.
  template <class Resource>
  static intrusive_ptr<producer_adapter>
  try_open(socket_manager* owner, Resource src) {
    CAF_ASSERT(owner != nullptr);
    if (auto buf = src.try_open()) {
      using ptr_type = intrusive_ptr<producer_adapter>;
      auto adapter = ptr_type{new producer_adapter(owner, buf), false};
      buf->set_producer(adapter);
      return adapter;
    } else {
      return nullptr;
    }
  }

  /// Makes `item` available to the consumer.
  /// @returns the remaining demand.
  size_t push(const value_type& item) {
    if (buf_) {
      return buf_->push(item);
    } else {
      return 0;
    }
  }

  /// Makes `items` available to the consumer.
  /// @returns the remaining demand.
  size_t push(span<const value_type> items) {
    if (buf_) {
      return buf_->push(items);
    } else {
      return 0;
    }
  }

  void close() {
    if (buf_) {
      buf_->close();
      buf_ = nullptr;
      mgr_ = nullptr;
    }
  }

  void abort(error reason) {
    if (buf_) {
      buf_->abort(std::move(reason));
      buf_ = nullptr;
      mgr_ = nullptr;
    }
  }

  friend void intrusive_ptr_add_ref(const producer_adapter* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const producer_adapter* ptr) noexcept {
    ptr->deref();
  }

private:
  producer_adapter(socket_manager* owner, buf_ptr buf)
    : mgr_(owner), buf_(std::move(buf)) {
    // nop
  }

  void on_cancel() {
    if (buf_) {
      mgr_->mpx().shutdown_reading(mgr_);
      buf_ = nullptr;
      mgr_ = nullptr;
    }
  }

  auto strong_this() {
    return intrusive_ptr{this};
  }

  intrusive_ptr<socket_manager> mgr_;
  intrusive_ptr<Buffer> buf_;
};

template <class T>
using producer_adapter_ptr = intrusive_ptr<producer_adapter<T>>;

} // namespace caf::net
