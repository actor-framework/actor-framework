// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <algorithm>
#include <utility>
#include <variant>

#include "caf/detail/overload.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/flow/observable.hpp"
#include "caf/ref_counted.hpp"

namespace caf::flow {

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

  void subscribe(observer<T> what) {
    if (pimpl_)
      pimpl_->subscribe(std::move(what));
    else
      what.on_error(make_error(sec::invalid_observable));
  }

  template <class OnSuccess, class OnError>
  void subscribe(OnSuccess on_success, OnError on_error) {
    static_assert(std::is_invocable_v<OnSuccess, const T&>);
    as_observable().for_each([f{std::move(on_success)}](
                               const T& item) mutable { f(item); },
                             std::move(on_error));
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

/// Convenience function for creating an @ref observable from a concrete
/// operator type.
template <class Operator, class... Ts>
single<typename Operator::output_type>
make_single(coordinator* ctx, Ts&&... xs) {
  using out_t = typename Operator::output_type;
  using ptr_t = intrusive_ptr<out_t>;
  ptr_t ptr{new Operator(ctx, std::forward<Ts>(xs)...), false};
  return single<out_t>{std::move(ptr)};
}

} // namespace caf::flow
