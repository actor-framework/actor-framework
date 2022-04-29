// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <memory>
#include <new>

#include "caf/async/producer.hpp"
#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"

namespace caf::net {

/// Connects a socket manager to an asynchronous producer resource.
template <class Buffer>
class producer_adapter final : public detail::atomic_ref_counted,
                               public async::producer {
public:
  using buf_ptr = intrusive_ptr<Buffer>;

  using value_type = typename Buffer::value_type;

  using ptr_type = intrusive_ptr<producer_adapter>;

  void on_consumer_ready() override {
    // nop
  }

  void on_consumer_cancel() override {
    mgr_->schedule(do_cancel_);
  }

  void on_consumer_demand(size_t) override {
    mgr_->schedule(do_resume_);
  }

  void ref_producer() const noexcept override {
    this->ref();
  }

  void deref_producer() const noexcept override {
    this->deref();
  }

  static ptr_type make(buf_ptr buf, socket_manager_ptr mgr, action do_resume,
                       action do_cancel) {
    if (buf) {
      auto adapter
        = ptr_type{new producer_adapter(buf, mgr, std::move(do_resume),
                                        std::move(do_cancel)),
                   false};
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
      reset();
    }
  }

  void abort(error reason) {
    if (buf_) {
      buf_->abort(std::move(reason));
      reset();
    }
  }

private:
  producer_adapter(buf_ptr buf, socket_manager_ptr mgr, action do_resume,
                   action do_cancel)
    : buf_(std::move(buf)),
      mgr_(std::move(mgr)),
      do_resume_(std::move(do_resume)),
      do_cancel_(std::move(do_cancel)) {
    // nop
  }

  void reset() {
    buf_ = nullptr;
    mgr_ = nullptr;
    do_resume_.dispose();
    do_resume_ = nullptr;
    do_cancel_.dispose();
    do_cancel_ = nullptr;
  }

  intrusive_ptr<Buffer> buf_;
  intrusive_ptr<socket_manager> mgr_;
  action do_resume_;
  action do_cancel_;
};

template <class T>
using producer_adapter_ptr = intrusive_ptr<producer_adapter<T>>;

} // namespace caf::net
