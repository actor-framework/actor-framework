// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/observable_decl.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/make_counted.hpp"

#include <algorithm>
#include <deque>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace caf::flow::op {

template <class F, class... Ts>
struct zip_with_oracle {
  using output_type
    = decltype(std::declval<F&>()(std::declval<const Ts&>()...));
};

template <class F, class... Ts>
using zip_with_output_t = typename zip_with_oracle<F, Ts...>::output_type;

template <size_t Index>
using zip_index = std::integral_constant<size_t, Index>;

template <class T>
struct zip_input {
  using value_type = T;

  subscription sub;
  std::deque<T> buf;

  T pop() {
    auto result = buf.front();
    buf.pop_front();
    return result;
  }

  /// Returns whether the input can no longer produce additional items.
  bool at_end() const noexcept {
    return !sub && buf.empty();
  }
};

/// Combines items from any number of observables using a zip function.
template <class F, class... Ts>
class zip_with_sub : public subscription::impl_base {
public:
  // -- member types -----------------------------------------------------------

  using output_type = zip_with_output_t<F, Ts...>;

  // -- constructors, destructors, and assignment operators --------------------

  zip_with_sub(coordinator* parent, F fn, observer<output_type> out,
               std::tuple<observable<Ts>...>& srcs)
    : parent_(parent), fn_(std::move(fn)), out_(std::move(out)) {
    for_each_input([this, &srcs](auto index, auto& input) {
      using index_type = decltype(index);
      using value_type = typename std::decay_t<decltype(input)>::value_type;
      using fwd_impl = forwarder<value_type, zip_with_sub, index_type>;
      auto fwd = parent_->add_child(std::in_place_type<fwd_impl>, this, index);
      std::get<index_type::value>(srcs).subscribe(fwd->as_observer());
    });
  }

  // -- implementation of subscription -----------------------------------------

  coordinator* parent() const noexcept override {
    return parent_;
  }

  bool disposed() const noexcept override {
    return !out_;
  }

  void request(size_t n) override {
    if (out_) {
      demand_ += n;
      for_each_input([n](auto, auto& input) {
        if (input.sub)
          input.sub.request(n);
      });
    }
  }

  // -- utility functions ------------------------------------------------------

  template <size_t I>
  auto& at(zip_index<I>) {
    return std::get<I>(inputs_);
  }

  template <class Fn, size_t... Is>
  void for_each_input(Fn&& fn, std::index_sequence<Is...>) {
    (fn(zip_index<Is>{}, at(zip_index<Is>{})), ...);
  }

  template <class Fn>
  void for_each_input(Fn&& fn) {
    for_each_input(std::forward<Fn>(fn), std::index_sequence_for<Ts...>{});
  }

  template <class Fn, size_t... Is>
  auto fold(Fn&& fn, std::index_sequence<Is...>) {
    return fn(at(zip_index<Is>{})...);
  }

  template <class Fn>
  auto fold(Fn&& fn) {
    return fold(std::forward<Fn>(fn), std::index_sequence_for<Ts...>{});
  }

  size_t buffered() {
    return fold([](auto&... x) { return std::min({x.buf.size()...}); });
  }

  // A zip reached the end if any of its inputs reached the end.
  bool at_end() {
    return fold([](auto&... inputs) { return (inputs.at_end() || ...); });
  }

  // -- callbacks for the forwarders -------------------------------------------

  template <size_t I>
  void fwd_on_subscribe(zip_index<I> index, subscription sub) {
    if (out_) {
      if (auto& in = at(index); !in.sub) {
        if (demand_ > 0)
          sub.request(demand_);
        in.sub = std::move(sub);
        return;
      }
    }
    sub.cancel();
  }

  template <size_t I>
  void fwd_on_complete(zip_index<I> index) {
    if (out_) {
      auto& input = at(index);
      if (input.sub)
        input.sub.release_later();
      if (input.buf.empty())
        fin();
    }
  }

  template <size_t I>
  void fwd_on_error(zip_index<I> index, const error& what) {
    if (out_) {
      if (!err_)
        err_ = what;
      auto& input = at(index);
      if (input.sub)
        input.sub.release_later();
      if (input.buf.empty())
        fin();
    }
  }

  template <size_t I, class T>
  void fwd_on_next(zip_index<I> index, const T& item) {
    if (out_) {
      at(index).buf.push_back(item);
      push();
    }
  }

private:
  void do_dispose(bool from_external) override {
    if (!out_)
      return;
    for_each_input([](auto, auto& input) {
      input.sub.cancel();
      input.buf.clear();
    });
    if (from_external) {
      if (!err_)
        err_ = error{sec::disposed};
      out_.on_error(err_);
    } else {
      out_.release_later();
    }
  }

  void push() {
    if (auto n = std::min(buffered(), demand_); n > 0) {
      demand_ -= n;
      for (size_t i = 0; i < n; ++i) {
        fold([this](auto&... x) { out_.on_next(fn_(x.pop()...)); });
        if (!out_) // on_next might call cancel()
          return;
      }
    }
    if (at_end())
      fin();
  }

  void fin() {
    for_each_input([](auto, auto& input) {
      input.sub.cancel();
      input.buf.clear();
    });
    // Set out_ to null and emit the final event.
    if (!err_)
      out_.on_complete();
    else
      out_.on_error(err_);
  }

  /// Stores the context (coordinator) that runs this flow.
  coordinator* parent_;

  /// Reduces n inputs to 1 output.
  F fn_;

  /// Stores the required state per input.
  std::tuple<zip_input<Ts>...> inputs_;

  /// Stores our current demand for items from the subscriber.
  size_t demand_ = 0;

  /// Stores the first error that occurred on any input.
  error err_;

  /// Stores a handle to the subscribed observer.
  observer<output_type> out_;
};

/// Combines items from any number of observables using a zip function.
template <class F, class... Ts>
class zip_with : public cold<zip_with_output_t<F, Ts...>> {
public:
  // -- member types -----------------------------------------------------------

  using output_type = zip_with_output_t<F, Ts...>;

  using super = cold<output_type>;

  // -- constructors, destructors, and assignment operators --------------------

  zip_with(coordinator* parent, F fn, observable<Ts>... inputs)
    : super(parent), fn_(std::move(fn)), inputs_(inputs...) {
    // nop
  }

  // -- implementation of observable<T>::impl ----------------------------------

  disposable subscribe(observer<output_type> out) override {
    using sub_t = zip_with_sub<F, Ts...>;
    auto ptr = super::parent_->add_child(std::in_place_type<sub_t>, fn_, out,
                                         inputs_);
    out.on_subscribe(subscription{ptr});
    return ptr->as_disposable();
  }

private:
  /// Reduces n inputs to 1 output.
  F fn_;

  /// Stores the source observables until an observer subscribes.
  std::tuple<observable<Ts>...> inputs_;
};

/// Creates a new zip-with operator from given inputs.
template <class F, class T0, class T1, class... Ts>
auto make_zip_with(coordinator* parent, F fn, T0 input0, T1 input1,
                   Ts... inputs) {
  using output_type = zip_with_output_t<F,                        //
                                        typename T0::output_type, //
                                        typename T1::output_type, //
                                        typename Ts::output_type...>;
  using impl_t = zip_with<F,                        //
                          typename T0::output_type, //
                          typename T1::output_type, //
                          typename Ts::output_type...>;
  if (input0.valid() && input1.valid() && (inputs.valid() && ...)) {
    auto ptr = parent->add_child(std::in_place_type<impl_t>, std::move(fn),
                                 std::move(input0).as_observable(),
                                 std::move(input1).as_observable(),
                                 std::move(inputs).as_observable()...);
    return observable<output_type>{std::move(ptr)};
  }
  return observable<output_type>{};
}

} // namespace caf::flow::op
