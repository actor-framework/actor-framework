// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/policy.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/observable_state.hpp"
#include "caf/flow/observer.hpp"

#include <type_traits>
#include <utility>

namespace caf::flow {

template <class F, class... Ts>
struct zipper_oracle {
  using output_type
    = decltype(std::declval<F&>()(std::declval<const Ts&>()...));
};

template <class F, class... Ts>
using zipper_output_t = typename zipper_oracle<F, Ts...>::output_type;

template <size_t Index>
using zipper_index = std::integral_constant<size_t, Index>;

template <class T>
struct zipper_input {
  using value_type = T;

  explicit zipper_input(observable<T> in) : in(std::move(in)) {
    // nop
  }

  observable<T> in;
  subscription sub;
  std::vector<T> buf;
};

/// Combines items from any number of observables using a zip function.
template <class F, class... Ts>
class zipper_impl : public ref_counted,
                    public observable_impl<zipper_output_t<F, Ts...>> {
public:
  // -- member types -----------------------------------------------------------

  using output_type = zipper_output_t<F, Ts...>;

  using super = observable_impl<output_type>;

  // -- friends ----------------------------------------------------------------

  CAF_INTRUSIVE_PTR_FRIENDS(zipper_impl)

  template <class, class, class>
  friend class forwarder;

  // -- constructors, destructors, and assignment operators --------------------

  zipper_impl(coordinator* ctx, F fn, observable<Ts>... inputs)
    : ctx_(ctx), fn_(std::move(fn)), inputs_(inputs...) {
    // nop
  }

  // -- implementation of disposable::impl -------------------------------------

  void dispose() override {
    broken_ = true;
    if (buffered() == 0) {
      fin();
    } else {
      for_each_input([](auto, auto& input) {
        input.in = nullptr;
        if (input.sub) {
          input.sub.cancel();
          input.sub = nullptr;
        }
        // Do not clear the buffer to allow already arrived items to go through.
      });
    }
  }

  bool disposed() const noexcept override {
    return term_.finalized();
  }

  void ref_disposable() const noexcept override {
    this->ref();
  }

  void deref_disposable() const noexcept override {
    this->deref();
  }

  // -- implementation of observable<T>::impl ----------------------------------

  coordinator* ctx() const noexcept override {
    return ctx_;
  }

  void on_request(observer_impl<output_type>* sink, size_t demand) override {
    if (auto n = term_.on_request(sink, demand); n > 0) {
      demand_ += n;
      for_each_input([n](auto, auto& input) {
        if (input.sub)
          input.sub.request(n);
      });
    }
  }

  void on_cancel(observer_impl<output_type>* sink) override {
    if (auto n = term_.on_cancel(sink); n > 0) {
      demand_ += n;
      for_each_input([n](auto, auto& input) {
        if (input.sub)
          input.sub.request(n);
      });
    }
  }

  disposable subscribe(observer<output_type> sink) override {
    // On the first subscribe, we subscribe to our inputs.
    auto res = term_.add(this, sink);
    if (res && term_.start()) {
      for_each_input([this](auto index, auto& input) {
        using input_t = std::decay_t<decltype(input)>;
        using value_t = typename input_t::value_type;
        using fwd_impl = forwarder<value_t, zipper_impl, decltype(index)>;
        auto fwd = make_counted<fwd_impl>(this, index);
        input.in.subscribe(fwd->as_observer());
      });
    }
    return res;
  }

private:
  template <size_t I>
  auto& at(zipper_index<I>) {
    return std::get<I>(inputs_);
  }

  template <class Fn, size_t... Is>
  void for_each_input(Fn&& fn, std::index_sequence<Is...>) {
    (fn(zipper_index<Is>{}, at(zipper_index<Is>{})), ...);
  }

  template <class Fn>
  void for_each_input(Fn&& fn) {
    for_each_input(std::forward<Fn>(fn), std::index_sequence_for<Ts...>{});
  }

  template <class Fn, size_t... Is>
  auto fold(Fn&& fn, std::index_sequence<Is...>) {
    return fn(at(zipper_index<Is>{})...);
  }

  template <class Fn>
  auto fold(Fn&& fn) {
    return fold(std::forward<Fn>(fn), std::index_sequence_for<Ts...>{});
  }

  size_t buffered() {
    return fold([](auto&... x) { return std::min({x.buf.size()...}); });
  }

  template <size_t I>
  void fwd_on_subscribe(zipper_index<I> index, subscription sub) {
    if (!term_.finalized()) {
      auto& in = at(index);
      if (!in.sub) {
        if (demand_ > 0)
          sub.request(demand_);
        in.sub = std::move(sub);
      } else {
        sub.cancel();
      }
    } else {
      sub.cancel();
    }
  }

  template <size_t I>
  void fwd_on_complete(zipper_index<I> index) {
    if (!broken_) {
      broken_ = true;
      if (at(index).buf.empty())
        fin();
    }
    at(index).sub = nullptr;
  }

  template <size_t I>
  void fwd_on_error(zipper_index<I> index, const error& what) {
    if (!term_.err())
      term_.err(what);
    if (!broken_) {
      broken_ = true;
      if (at(index).buf.empty())
        fin();
    }
    at(index).sub = nullptr;
  }

  template <size_t I, class T>
  void fwd_on_next(zipper_index<I> index, span<const T> items) {
    if (!term_.finalized()) {
      auto& buf = at(index).buf;
      buf.insert(buf.end(), items.begin(), items.end());
      push();
    }
  }

  void push() {
    if (auto n = std::min(buffered(), demand_); n > 0) {
      for (size_t index = 0; index < n; ++index) {
        fold([this, index](auto&... x) { //
          term_.on_next(fn_(x.buf[index]...));
        });
      }
      demand_ -= n;
      for_each_input([n](auto, auto& x) { //
        x.buf.erase(x.buf.begin(), x.buf.begin() + n);
      });
      term_.push();
      if (broken_ && buffered() == 0)
        fin();
    }
  }

  void fin() {
    for_each_input([](auto, auto& input) {
      input.in = nullptr;
      if (input.sub) {
        input.sub.cancel();
        input.sub = nullptr;
      }
      input.buf.clear();
    });
    term_.fin();
  }

  coordinator* ctx_;
  size_t demand_ = 0;
  F fn_;
  std::tuple<zipper_input<Ts>...> inputs_;

  /// A zipper breaks as soon as one of its inputs signals completion or error.
  bool broken_ = false;

  broadcast_step<output_type> term_;
};

template <class F, class... Ts>
using zipper_impl_ptr = intrusive_ptr<zipper_impl<F, Ts...>>;

/// @param fn The zip function. Takes one element from each input at a time and
///           converts them into a single result.
/// @param input0 The input at index 0.
/// @param input1 The input at index 1.
/// @param inputs The inputs for index > 1.
template <class F, class T0, class T1, class... Ts>
auto zip_with(F fn, T0 input0, T1 input1, Ts... inputs) {
  using output_type = zipper_output_t<F, typename T0::output_type, //
                                      typename T1::output_type,    //
                                      typename Ts::output_type...>;
  using impl_t = zipper_impl<F,                        //
                             typename T0::output_type, //
                             typename T1::output_type, //
                             typename Ts::output_type...>;
  if (input0.valid() && input1.valid() && (inputs.valid() && ...)) {
    auto ctx = input0.ctx();
    auto ptr = make_counted<impl_t>(ctx, std::move(fn),
                                    std::move(input0).as_observable(),
                                    std::move(input1).as_observable(),
                                    std::move(inputs).as_observable()...);
    return observable<output_type>{std::move(ptr)};
  } else {
    return observable<output_type>{};
  }
}

} // namespace caf::flow
