// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/lower_layer.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/actor_system.hpp"
#include "caf/async/consumer_adapter.hpp"
#include "caf/async/future.hpp"
#include "caf/async/producer_adapter.hpp"
#include "caf/async/promise.hpp"
#include "caf/async/publisher.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/op/ucast.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/sec.hpp"
#include "caf/settings.hpp"

#include <cstddef>
#include <future>
#include <new>
#include <type_traits>
#include <utility>

namespace caf::detail {

/// Trivial observer that forwards all events to a
/// `net::octet_stream::flow_bridge`.
class CAF_NET_EXPORT octet_stream_observer
  : public flow::observer_impl_base<std::byte> {
public:
  using input_type = std::byte;

  octet_stream_observer(flow::coordinator* parent,
                        net::octet_stream::flow_bridge* listener)
    : parent_(parent), listener_(listener) {
    // nop
  }

  flow::coordinator* parent() const noexcept override;

  void on_next(const std::byte& item) override;

  void on_error(const error&) override;

  void on_complete() override;

  void on_subscribe(flow::subscription new_sub) override;

private:
  flow::coordinator* parent_;
  net::octet_stream::flow_bridge* listener_;
};

} // namespace caf::detail

namespace caf::net::octet_stream {

using ucast_sub_state = flow::op::ucast_sub_state<std::byte>;

/// Translates between a byte-oriented transport and data flows. Utility class
/// for the `with` DSL.
class CAF_NET_EXPORT flow_bridge : public upper_layer,
                                   public ucast_sub_state::abstract_listener {
public:
  flow_bridge(uint32_t read_buffer_size, uint32_t write_buffer_size)
    : read_buffer_size_(read_buffer_size),
      write_buffer_size_(write_buffer_size) {
    // nop
  }

  template <class MakeInputs, class MakeOutputs>
  static auto make(uint32_t read_buffer_size, uint32_t write_buffer_size,
                   MakeInputs make_inputs, MakeOutputs make_outputs);

  void on_subscribed(ucast_sub_state* state) override;

  void on_disposed(ucast_sub_state*, bool from_external) override;

  void on_consumed_some(ucast_sub_state*, size_t,
                        size_t new_buffer_size) override;

  virtual void init(socket_manager* ptr);

  error start(lower_layer* down) override;

  void prepare_send() override;

  bool done_sending() override;

  void abort(const error& reason) override;

  ptrdiff_t consume(byte_span buf, byte_span) override;

  void written(size_t) override;

  void on_next(std::byte item);

  void on_error(const error& what);

  void on_complete();

  void on_subscribe(flow::subscription sub);

protected:
  virtual void on_start() = 0;

  /// The socket manager that owns this flow bridge.
  socket_manager* self_ = nullptr;

  /// The maximum size of the read buffer.
  uint32_t read_buffer_size_ = 0;

  /// The maximum size of the write buffer.
  uint32_t write_buffer_size_ = 0;

  /// Points to the next layer down the protocol stack.
  lower_layer* down_ = nullptr;

  /// The flow that consumes the bytes we receive from the lower layer.
  flow::op::ucast_ptr<std::byte> in_;

  /// The subscription for the flow that generates the bytes to send.
  flow::subscription out_;

  /// Stores how many bytes we have requested from `out_`.
  size_t requested_ = 0;

  /// Stores excess bytes from `out_` that exceeded the assigned capacity.
  size_t overflow_ = 0;
};

/// Adapter for constructing a `publisher<T>` that forwards subscriptions to a
/// flow bridge.
template <class T>
class flow_bridge_adapter : public detail::flow_source<T> {
public:
  flow_bridge_adapter(async::execution_context_ptr ctx) : ctx_(std::move(ctx)) {
    // nop
  }

  void add(async::producer_resource<T> sink) override {
    ctx_->schedule_fn([sink = std::move(sink), thisptr = shared()]() mutable {
      thisptr->do_add(std::move(sink));
    });
  }

  void init(flow::observable<T>* inputs) {
    inputs_ = inputs;
  }

  void close() {
    inputs_ = nullptr;
  }

  void abort(error reason) {
    inputs_ = nullptr;
    error_ = std::move(reason);
  }

private:
  void do_add(async::producer_resource<T> sink) {
    if (auto ptr = inputs_)
      ptr->subscribe(std::move(sink));
    else if (error_)
      sink.abort(error_);
    else
      sink.close();
  }

  intrusive_ptr<flow_bridge_adapter> shared() {
    return this;
  }

  async::execution_context_ptr ctx_;

  flow::observable<T>* inputs_ = nullptr;

  error error_;
};

/// Forwards only the `on_complete` or `on_error` event to subscribers.
template <class T>
class flow_bridge_signalizer : public detail::flow_source<T> {
public:
  void add(async::producer_resource<T> sink) override {
    std::lock_guard guard{mtx_};
    if (closed_) {
      if (error_)
        sink.abort(error_);
      else
        sink.close();
      return;
    }
    sinks_.push_back(std::move(sink));
  }

  void close() {
    std::lock_guard guard{mtx_};
    closed_ = true;
    for (auto& sink : sinks_)
      sink.close();
    sinks_.clear();
  }

  void abort(error reason) {
    std::lock_guard guard{mtx_};
    closed_ = true;
    error_ = std::move(reason);
    for (auto& sink : sinks_)
      sink.abort(error_);
    sinks_.clear();
  }

private:
  std::mutex mtx_;

  std::vector<async::producer_resource<T>> sinks_;

  bool closed_ = false;

  error error_;
};

/// Implementation detail for `flow_bridge_inputs_t`.
template <class MakeInputsResult>
struct flow_bridge_inputs_oracle {
  using type = typename MakeInputsResult::output_type;
};

template <>
struct flow_bridge_inputs_oracle<void> {
  using type = void;
};

/// Extracts the item type from a `MakeInputs` callback or `void` if the
/// callback does not return an observable.
template <class MakeInputs>
using flow_bridge_inputs_t = typename flow_bridge_inputs_oracle<
  decltype(std::declval<MakeInputs&>()(flow::observable<std::byte>{}))>::type;

/// Provides boilerplate code for flow bridges that can be subscribed to via a
/// publisher.
template <class T>
class flow_bridge_impl_base : public flow_bridge {
public:
  using super = flow_bridge;

  using publisher_type = async::publisher<T>;

  flow_bridge_impl_base(uint32_t read_buffer_size, uint32_t write_buffer_size)
    : super(read_buffer_size, write_buffer_size) {
  }

  ~flow_bridge_impl_base() {
    source_->close();
  }

  void init(socket_manager* ptr) override {
    source_ = make_counted<flow_bridge_adapter<T>>(ptr);
    super::init(ptr);
  }

  async::publisher<T> publisher() {
    return async::publisher<T>::from(source_);
  }

protected:
  template <class Init>
  void init_inputs(Init&& inputs) {
    inputs_ = std::forward<Init>(inputs).as_observable();
    source_->init(&inputs_);
  }

private:
  flow::observable<T> inputs_;
  caf::intrusive_ptr<flow_bridge_adapter<T>> source_;
};

/// Specialization for flow bridges that do not have an input flow.
template <>
class flow_bridge_impl_base<void> : public flow_bridge {
public:
  static constexpr bool has_publisher = false;

  using super = flow_bridge;

  using publisher_type = async::publisher<unit_t>;

  using super::super;

  ~flow_bridge_impl_base() {
    source_->close();
  }

  void init(socket_manager* ptr) override {
    source_ = make_counted<flow_bridge_signalizer<unit_t>>();
    super::init(ptr);
  }

  publisher_type publisher() const {
    return async::publisher<unit_t>::from(source_);
  }

protected:
  caf::intrusive_ptr<flow_bridge_signalizer<unit_t>> source_;
};

/// Implements the flow bridge with two factory functions for creating the input
/// and output flows.
template <class MakeInputs, class MakeOutputs>
class flow_bridge_impl
  : public flow_bridge_impl_base<flow_bridge_inputs_t<MakeInputs>> {
public:
  using input_type = flow_bridge_inputs_t<MakeInputs>;

  using super = flow_bridge_impl_base<input_type>;

  flow_bridge_impl(uint32_t read_buffer_size, uint32_t write_buffer_size,
                   MakeInputs&& make_inputs, MakeOutputs&& make_outputs)
    : super(read_buffer_size, write_buffer_size) {
    new (&make_inputs_) MakeInputs(std::move(make_inputs));
    new (&make_outputs_) MakeOutputs(std::move(make_outputs));
  }

  ~flow_bridge_impl() {
    if (!started) {
      make_inputs_.~MakeInputs();
      make_outputs_.~MakeOutputs();
    }
  }

private:
  void on_start() override {
    CAF_ASSERT(!started);
    auto self = static_cast<flow::coordinator*>(super::self_);
    auto obs_ptr = make_counted<detail::octet_stream_observer>(self, this);
    auto obs = flow::observer<std::byte>{std::move(obs_ptr)};
    make_outputs_(self).subscribe(std::move(obs));
    if constexpr (std::is_void_v<input_type>) {
      auto src = super::source_;
      make_inputs_(
        flow::observable<std::byte>{super::in_}
          .do_on_complete([src] { src->close(); })
          .do_on_error([src](error reason) { src->abort(std::move(reason)); })
          .as_observable());
    } else {
      this->init_inputs(make_inputs_(flow::observable<std::byte>{super::in_}));
    }
    started = true;
    make_inputs_.~MakeInputs();
    make_outputs_.~MakeOutputs();
  }

  bool started = false;

  union {
    MakeInputs make_inputs_;
  };

  union {
    MakeOutputs make_outputs_;
  };
};

template <class MakeInputs, class MakeOutputs>
auto flow_bridge::make(uint32_t read_buffer_size, uint32_t write_buffer_size,
                       MakeInputs make_inputs, MakeOutputs make_outputs) {
  using res_t = decltype(make_inputs(flow::observable<std::byte>{}));
  static_assert(std::is_same_v<res_t, void> || flow::is_observable_v<res_t>,
                "MakeInputs must return void or an observable");
  using out_t
    = decltype(make_outputs(static_cast<flow::coordinator*>(nullptr)));
  static_assert(flow::is_observable_v<out_t>,
                "MakeOutputs must return an observable emitting std::byte");
  static_assert(std::is_same_v<typename out_t::output_type, std::byte>,
                "MakeOutputs must return an observable emitting std::byte");
  using impl_t = flow_bridge_impl<MakeInputs, MakeOutputs>;
  return std::make_unique<impl_t>(read_buffer_size, write_buffer_size,
                                  std::move(make_inputs),
                                  std::move(make_outputs));
}

template <class MakeInputs, class MakeOutputs>
using flow_bridge_publisher_t =
  typename flow_bridge_impl<MakeInputs, MakeOutputs>::publisher_type;

} // namespace caf::net::octet_stream
