// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/observable_decl.hpp"
#include "caf/flow/op/fail.hpp"

namespace caf::async {

/// Provides an interface for accessing an asynchronous data flow. Unlike a @ref
/// future, a publisher produces multiple values over time. Subscribers will
/// only receive items that are emitted after they have subscribed to the
/// publisher.
template <class T>
class publisher {
public:
  publisher() noexcept = default;

  publisher(publisher&&) noexcept = default;

  publisher(const publisher&) noexcept = default;

  publisher& operator=(publisher&&) noexcept = default;

  publisher& operator=(const publisher&) noexcept = default;

  /// Creates a @ref flow::observable that reads and emits all values from this
  /// publisher.
  flow::observable<T> observe_on(flow::coordinator* ctx, size_t buffer_size,
                                 size_t min_request_size) const {
    if (impl_)
      return impl_->observe_on(ctx, buffer_size, min_request_size);
    auto err = make_error(sec::invalid_observable,
                          "cannot subscribe to default-constructed observable");
    // Note: cannot use ctx->make_observable() here because it would create a
    //       circular dependency between observable_builder and publisher.
    return flow::make_observable<flow::op::fail<T>>(ctx, std::move(err));
  }

  /// Creates a @ref flow::observable that reads and emits all values from this
  /// publisher.
  flow::observable<T> observe_on(flow::coordinator* ctx) const {
    return observe_on(ctx, defaults::flow::buffer_size,
                      defaults::flow::min_demand);
  }

  /// Creates a new asynchronous observable by decorating a regular observable.
  static publisher from(flow::observable<T> decorated) {
    if (!decorated)
      return {};
    auto* ctx = decorated.ctx();
    auto flag = disposable::make_flag();
    ctx->watch(flag);
    auto pimpl = make_counted<impl>(ctx, std::move(decorated), std::move(flag));
    return publisher{std::move(pimpl)};
  }

private:
  class impl : public ref_counted {
  public:
    impl(async::execution_context_ptr source, flow::observable<T> decorated,
         disposable flag)
      : source_(std::move(source)),
        decorated_(std::move(decorated)),
        flag_(std::move(flag)) {
      // nop
    }

    flow::observable<T> observe_on(flow::coordinator* ctx, size_t buffer_size,
                                   size_t min_request_size) const {
      // Short circuit if we are already on the target coordinator.
      if (ctx == source_.get())
        return decorated_;
      // Otherwise, create a new SPSC buffer and connect it to the source.
      auto [pull, push] = async::make_spsc_buffer_resource<T>(buffer_size,
                                                              min_request_size);
      source_->schedule_fn(
        [push = std::move(push), decorated = decorated_]() mutable {
          decorated.subscribe(std::move(push));
        });
      return pull.observe_on(ctx);
    }

    ~impl() {
      source_->schedule_fn([flag = std::move(flag_)]() mutable {
        // The source called `watch` on the flag to keep the event loop alive as
        // long as there are still async references to this observable. We need
        // to dispose the flag in the event loop of the source in order to make
        // sure that the source cleans up properly.
        flag.dispose();
      });
    }

  private:
    async::execution_context_ptr source_;
    flow::observable<T> decorated_;
    disposable flag_;
  };

  using impl_ptr = caf::intrusive_ptr<impl>;

  explicit publisher(impl_ptr pimpl) : impl_(std::move(pimpl)) {
    // nop
  }

  impl_ptr impl_;
};

} // namespace caf::async
