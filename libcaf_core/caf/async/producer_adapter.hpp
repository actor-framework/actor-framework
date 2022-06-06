// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/execution_context.hpp"
#include "caf/async/producer.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"

#include <optional>

namespace caf::async {

/// Integrates an SPSC buffer producer into an asynchronous event loop.
template <class T>
class producer_adapter {
public:
  class impl : public detail::atomic_ref_counted, public producer {
  public:
    impl() = delete;
    impl(const impl&) = delete;
    impl& operator=(const impl&) = delete;

    impl(spsc_buffer_ptr<T> buf, execution_context_ptr ctx, action do_resume,
         action do_cancel)
      : buf_(std::move(buf)),
        ctx_(std::move(ctx)),
        do_resume_(std::move(do_resume)),
        do_cancel_(std::move(do_cancel)) {
      buf_->set_producer(this);
    }

    size_t push(span<const T> items) {
      return buf_->push(items);
    }

    bool push(const T& item) {
      return buf_->push(item);
    }

    void close() {
      if (buf_) {
        buf_->close();
        buf_ = nullptr;
        do_resume_.dispose();
        do_resume_ = nullptr;
        do_cancel_.dispose();
        do_cancel_ = nullptr;
      }
    }

    void abort(error reason) {
      if (buf_) {
        buf_->abort(std::move(reason));
        buf_ = nullptr;
        do_resume_.dispose();
        do_resume_ = nullptr;
        do_cancel_.dispose();
        do_cancel_ = nullptr;
      }
    }

    void on_consumer_ready() override {
      // nop
    }

    void on_consumer_cancel() override {
      ctx_->schedule(do_cancel_);
    }

    void on_consumer_demand(size_t) override {
      ctx_->schedule(do_resume_);
    }

    void ref_producer() const noexcept override {
      ref();
    }

    void deref_producer() const noexcept override {
      deref();
    }

  private:
    spsc_buffer_ptr<T> buf_;
    execution_context_ptr ctx_;
    action do_resume_;
    action do_cancel_;
  };

  using impl_ptr = intrusive_ptr<impl>;

  producer_adapter() = default;

  producer_adapter(const producer_adapter&) = delete;

  producer_adapter& operator=(const producer_adapter&) = delete;

  producer_adapter(producer_adapter&&) = default;

  producer_adapter& operator=(producer_adapter&&) = default;

  explicit producer_adapter(impl_ptr ptr) : impl_(std::move(ptr)) {
    // nop
  }

  ~producer_adapter() {
    if (impl_)
      impl_->close();
  }

  /// Makes `item` available to the consumer.
  /// @returns the remaining demand.
  size_t push(const T& item) {
    if (!impl_)
      CAF_RAISE_ERROR("cannot push to a closed producer adapter");
    return impl_->push(item);
  }

  /// Makes `items` available to the consumer.
  /// @returns the remaining demand.
  size_t push(span<const T> items) {
    if (!impl_)
      CAF_RAISE_ERROR("cannot push to a closed producer adapter");
    return impl_->push(items);
  }

  void close() {
    if (impl_) {
      impl_->close();
      impl_ = nullptr;
    }
  }

  void abort(error reason) {
    if (impl_) {
      impl_->abort(std::move(reason));
      impl_ = nullptr;
    }
  }

  explicit operator bool() const noexcept {
    return static_cast<bool>(impl_);
  }

  /// @pre `buf != nullptr`
  static producer_adapter make(spsc_buffer_ptr<T> buf,
                               execution_context_ptr ctx, action do_resume,
                               action do_cancel) {
    if (buf) {
      using impl_t = typename producer_adapter<T>::impl;
      auto impl = make_counted<impl_t>(std::move(buf), std::move(ctx),
                                       std::move(do_resume),
                                       std::move(do_cancel));
      return producer_adapter<T>{std::move(impl)};
    } else {
      return {};
    }
  }

  static std::optional<producer_adapter>
  make(producer_resource<T> res, execution_context_ptr ctx, action do_resume,
       action do_cancel) {
    if (auto buf = res.try_open()) {
      return {make(std::move(buf), std::move(ctx), std::move(do_resume),
                   std::move(do_cancel))};
    } else {
      return {};
    }
  }

private:
  intrusive_ptr<impl> impl_;
};

/// @pre `buf != nullptr`
/// @relates producer_adapter
template <class T>
producer_adapter<T> make_producer_adapter(spsc_buffer_ptr<T> buf,
                                          execution_context_ptr ctx,
                                          action do_resume, action do_cancel) {
  return producer_adapter<T>::make(std::move(buf), std::move(ctx),
                                   std::move(do_resume), std::move(do_cancel));
}

/// @relates producer_adapter
template <class T>
std::optional<producer_adapter<T>>
make_producer_adapter(producer_resource<T> res, execution_context_ptr ctx,
                      action do_resume, action do_cancel) {
  if (auto buf = res.try_open()) {
    return {producer_adapter<T>::make(std::move(buf), std::move(ctx),
                                      std::move(do_resume),
                                      std::move(do_cancel))};
  } else {
    return {};
  }
}

} // namespace caf::async
