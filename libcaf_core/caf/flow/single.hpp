// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/overload.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/flow/observable.hpp"
#include "caf/ref_counted.hpp"

#include <algorithm>
#include <optional>
#include <type_traits>
#include <utility>

namespace caf::flow {

template <class OnSuccess>
struct on_success_orcacle {
  using trait = detail::get_callable_trait_t<OnSuccess>;

  static_assert(trait::num_args == 1,
                "OnSuccess functions must take exactly one argument");

  using arg_type = std::decay_t<detail::tl_head_t<typename trait::arg_types>>;
};

template <class OnSuccess>
using on_success_arg_t = typename on_success_orcacle<OnSuccess>::arg_type;

template <class OnSuccess, class OnError>
class single_observer_impl
  : public observer_impl_base<on_success_arg_t<OnSuccess>> {
public:
  using input_type = on_success_arg_t<OnSuccess>;

  single_observer_impl(coordinator* parent, OnSuccess on_success,
                       OnError on_error)
    : parent_(parent),
      on_success_(std::move(on_success)),
      on_error_(std::move(on_error)) {
    // nop
  }

  coordinator* parent() const noexcept override {
    return parent_;
  }

  void on_subscribe(subscription sub) override {
    // Request one additional item to detect whether the observable emits more
    // than one item.
    sub.request(2);
    sub_ = std::move(sub);
  }

  void on_next(const input_type& item) override {
    if (!result_) {
      result_.emplace(item);
      return;
    }
    sub_.cancel();
    auto err = make_error(sec::runtime_error,
                          "caf::flow::single emitted more than one item");
    on_error_(err);
  }

  void on_complete() override {
    if (!sub_)
      return;
    sub_.release_later();
    if (result_) {
      on_success_(*result_);
      result_ = std::nullopt;
      return;
    }
    auto err = make_error(sec::broken_promise,
                          "caf::flow::single failed to produce an item");
    on_error_(err);
  }

  void on_error(const error& what) override {
    if (!sub_)
      return;
    sub_.release_later();
    on_error_(what);
  }

private:
  coordinator* parent_;
  OnSuccess on_success_;
  OnError on_error_;
  std::optional<input_type> result_;
  subscription sub_;
};

/// Similar to an `observable`, but always emits either a single value or an
/// error.
template <class T>
class single {
public:
  using output_type = T;

  explicit single(intrusive_ptr<op::base<T>> pimpl) noexcept
    : pimpl_(std::move(pimpl)) {
    // nop
  }

  single& operator=(std::nullptr_t) noexcept {
    pimpl_.reset();
    return *this;
  }

  single() noexcept = default;
  single(single&&) noexcept = default;
  single(const single&) noexcept = default;
  single& operator=(single&&) noexcept = default;
  single& operator=(const single&) noexcept = default;

  observable<T> as_observable() && {
    return observable<T>{std::move(pimpl_)};
  }
  observable<T> as_observable() const& {
    return observable<T>{pimpl_};
  }

  template <class OnSuccess, class OnError>
  disposable subscribe(OnSuccess on_success, OnError on_error) {
    static_assert(std::is_invocable_v<OnSuccess, const T&>);
    static_assert(std::is_invocable_v<OnError, const error&>);
    using impl_t = single_observer_impl<OnSuccess, OnError>;
    auto hdl = pimpl_->parent()->add_child_hdl(std::in_place_type<impl_t>,
                                               std::move(on_success),
                                               std::move(on_error));
    return pimpl_->subscribe(std::move(hdl));
  }

  bool valid() const noexcept {
    return pimpl_ != nullptr;
  }

  explicit operator bool() const noexcept {
    return valid();
  }

  bool operator!() const noexcept {
    return !valid();
  }

  void swap(single& other) {
    pimpl_.swap(other.pimpl_);
  }

private:
  intrusive_ptr<op::base<T>> pimpl_;
};

/// Convenience function for creating a @ref single from a flow operator.
template <class Operator, class... Ts>
single<typename Operator::output_type>
make_single(coordinator* ctx, Ts&&... xs) {
  using out_t = typename Operator::output_type;
  using ptr_t = intrusive_ptr<out_t>;
  ptr_t ptr{new Operator(ctx, std::forward<Ts>(xs)...), false};
  return single<out_t>{std::move(ptr)};
}

} // namespace caf::flow
