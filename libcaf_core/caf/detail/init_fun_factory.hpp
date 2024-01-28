// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/behavior.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/spawn_fwd.hpp"
#include "caf/detail/unique_function.hpp"
#include "caf/fwd.hpp"

#include <tuple>

namespace caf::detail {

class CAF_CORE_EXPORT init_fun_factory_helper_base
  : public unique_function<behavior(local_actor*)>::wrapper {
public:
  // -- member types -----------------------------------------------------------

  using super = unique_function<behavior(local_actor*)>::wrapper;

  using hook_fun_type = unique_function<void(local_actor*)>;

  // -- constructors, destructors, and assignment operators --------------------

  using super::super;

  // -- properties -------------------------------------------------------------

  template <class T>
  void hook(T&& x) {
    hook_ = hook_fun_type{std::forward<T>(x)};
  }

protected:
  // -- member variables -------------------------------------------------------

  hook_fun_type hook_;
};

/// Wraps a user-defined function and gives it a uniform signature.
template <class Base, class F, class Tuple, bool ReturnsBehavior,
          bool HasSelfPtr>
class init_fun_factory_helper final : public init_fun_factory_helper_base {
public:
  using args_pointer = std::shared_ptr<Tuple>;

  static constexpr bool args_empty = std::tuple_size_v<Tuple> == 0;

  init_fun_factory_helper(F fun, args_pointer args)
    : fun_(std::move(fun)), args_(std::move(args)) {
    // nop
  }

  init_fun_factory_helper(init_fun_factory_helper&&) = default;

  init_fun_factory_helper& operator=(init_fun_factory_helper&&) = default;

  behavior operator()(local_actor* self) final {
    if (hook_ != nullptr)
      hook_(self);
    [[maybe_unused]] auto dptr = static_cast<Base*>(self);
    if constexpr (ReturnsBehavior) {
      auto unbox = [](auto x) -> behavior { return std::move(x.unbox()); };
      if constexpr (args_empty) {
        if constexpr (HasSelfPtr) {
          // behavior (pointer)
          return unbox(fun_(dptr));
        } else {
          // behavior ()
          return unbox(fun_());
        }
      } else {
        if constexpr (HasSelfPtr) {
          // behavior (pointer, args...)
          auto res = apply_moved_args_prefixed(fun_, get_indices(*args_),
                                               *args_, dptr);
          return unbox(std::move(res));
        } else {
          // behavior (args...)
          return unbox(apply_args(fun_, get_indices(*args_), *args_));
        }
      }
    } else {
      if constexpr (args_empty) {
        if constexpr (HasSelfPtr) {
          // void (pointer)
          fun_(dptr);
        } else {
          // void ()
          fun_();
        }
      } else {
        if constexpr (HasSelfPtr) {
          // void (pointer, args...)
          apply_moved_args_prefixed(fun_, get_indices(*args_), *args_, dptr);
        } else {
          // void (args...)
          apply_args(fun_, get_indices(*args_), *args_);
        }
      }
      return {};
    }
  }

  F fun_;
  args_pointer args_;
};

template <class Base, class F>
class init_fun_factory {
public:
  static_assert(std::is_base_of_v<local_actor, Base>,
                "Base does not extend local_actor");

  using ptr_type = std::unique_ptr<init_fun_factory_helper_base>;

  using fun = unique_function<behavior(local_actor*)>;

  template <class... Ts>
  ptr_type make(F f, Ts&&... xs) {
    using trait = detail::get_callable_trait_t<F>;
    using arg_types = typename trait::arg_types;
    using res_type = typename trait::result_type;
    using first_arg = detail::tl_head_t<arg_types>;
    constexpr bool selfptr = std::is_pointer_v<first_arg>;
    constexpr bool rets = std::is_convertible_v<res_type, behavior>;
    using tuple_type = decltype(std::make_tuple(detail::spawn_fwd<Ts>(xs)...));
    using helper = init_fun_factory_helper<Base, F, tuple_type, rets, selfptr>;
    return ptr_type{new helper{std::move(f), sizeof...(Ts) > 0
                                               ? std::make_shared<tuple_type>(
                                                 detail::spawn_fwd<Ts>(xs)...)
                                               : nullptr}};
  }

  template <class... Ts>
  fun operator()(F f, Ts&&... xs) {
    return fun{make(std::move(f), std::forward<Ts>(xs)...).release()};
  }
};

template <class Base, class State>
class init_fun_factory<Base, actor_from_state_t<State>> {
public:
  static_assert(std::is_base_of_v<local_actor, Base>,
                "Base does not extend local_actor");

  using fun = unique_function<behavior(local_actor*)>;

  template <class... Ts>
  fun operator()(actor_from_state_t<State> f, Ts&&... xs) {
    using impl_t = typename actor_from_state_t<State>::impl_type;
    if constexpr (sizeof...(Ts) > 0) {
      return fun{[f, args = std::make_tuple(detail::spawn_fwd<Ts>(xs)...)] //
                 (local_actor * self) mutable {
                   return std::apply(
                     [self, f](auto&&... xs) {
                       return f(static_cast<impl_t*>(self),
                                std::forward<decltype(xs)>(xs)...)
                         .unbox();
                     },
                     std::move(args));
                 }};
    } else {
      return fun{[f](local_actor* self) mutable {
        return f(static_cast<impl_t*>(self)).unbox();
      }};
    }
  }
};

} // namespace caf::detail
