/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_VARIANT_HPP
#define CAF_VARIANT_HPP

#include <type_traits>

#include "caf/config.hpp"
#include "caf/fwd.hpp"
#include "caf/static_visitor.hpp"

#include "caf/meta/omittable.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/variant_data.hpp"

#define CAF_VARIANT_CASE(n)                                                    \
  case n:                                                                      \
    return f(std::forward<Us>(xs)...,                                          \
             x.get(std::integral_constant<int, (n <= max_type_id ? n : 0)>()))

#define CAF_VARIANT_DISPATCH_CASE(n)                                           \
  case n:                                                                      \
    return next.template apply_impl<Result>(                                   \
      std::forward<Variant>(next), std::forward<Visitor>(f),                   \
      std::forward<Us>(xs)...,                                                 \
      x.get(std::integral_constant<int, (n <= max_type_id ? n : 0)>()))

#define CAF_VARIANT_ASSIGN_CASE(n)                                             \
  case n: {                                                                    \
    using tmp_t = typename detail::tl_at<                                      \
                    detail::type_list<Ts...>,                                  \
                    (n < sizeof...(Ts) ? n : 0)                                \
                  >::type;                                                     \
    x.x = tmp_t{};                                                             \
    return f(get<tmp_t>(x.x));                                                 \
  }

namespace caf {

constexpr size_t variant_npos = static_cast<size_t>(-1);

struct variant_marker_t {};

constexpr variant_marker_t variant_marker = variant_marker_t{};

template <class T>
struct variant_assign_helper {
  using result_type = void;
  T& lhs;
  variant_assign_helper(T& lhs_ref) : lhs(lhs_ref) { }
  template <class U>
  void operator()(const U& rhs) const {
    lhs = rhs;
  }
};

template <class T>
struct variant_move_helper {
  using result_type = void;
  T& lhs;
  variant_move_helper(T& lhs_ref) : lhs(lhs_ref) { }
  template <class U>
  void operator()(U& rhs) const {
    lhs = std::move(rhs);
  }
};

template <class F, class... Ts>
struct variant_visit_result {
  using type =
    decltype((std::declval<F&>())(std::declval<typename Ts::type0&>()...));
};

template <class F, class... Ts>
using variant_visit_result_t =
  typename variant_visit_result<detail::decay_t<F>,
                                detail::decay_t<Ts>...>::type;

template <class T, class U,
          bool Enable = std::is_integral<T>::value
                        && std::is_integral<U>::value
                        && !std::is_same<T, bool>::value>
struct is_equal_int_type {
  static constexpr bool value = sizeof(T) == sizeof(U)
                                && std::is_signed<T>::value
                                   == std::is_signed<U>::value;
};

template <class T, typename U>
struct is_equal_int_type<T, U, false> : std::false_type { };

/// Compares `T` to `U` und evaluates to `true_type` if either
/// `T == U or if T and U are both integral types of the
/// same size and signedness. This works around the issue that
/// `uint8_t != unsigned char on some compilers.
template <class T, typename U>
struct is_same_ish
    : std::conditional<
        std::is_same<T, U>::value,
        std::true_type,
        is_equal_int_type<T, U>
      >::type { };

template <class T>
struct is_variant : std::false_type {};

template <class... Ts>
struct is_variant<variant<Ts...>> : std::true_type {};

template <class... Ts>
struct is_variant<variant<Ts...>&> : std::true_type {};

template <class... Ts>
struct is_variant<const variant<Ts...>&> : std::true_type {};

template <class... Ts>
struct is_variant<const variant<Ts...>&&> : std::true_type {};

/// A variant represents always a valid value of one of the types `Ts...`.
template <class... Ts>
class variant {
public:
  using types = detail::type_list<Ts...>;

  static constexpr int max_type_id = sizeof...(Ts) - 1;

  static_assert(sizeof...(Ts) <= 20, "Too many template arguments given.");

  static_assert(sizeof...(Ts) > 0, "No template argument given.");

  static_assert(!detail::tl_exists<types, std::is_reference>::value,
                "Cannot create a variant of references");

  using type0 = typename detail::tl_at<types, 0>::type;

  variant& operator=(const variant& other) {
    variant_assign_helper<variant> helper{*this};
    other.template apply<void>(helper);
    return *this;
  }

  variant& operator=(variant&& other) {
    variant_move_helper<variant> helper{*this};
    other.template apply<void>(helper);
    return *this;
  }

  variant() : type_(variant_npos) {
    // Never empty ...
    set(typename detail::tl_head<types>::type());
    // ... unless an exception was thrown above.
    type_ = 0;
  }

  template <class U>
  variant(U&& arg) : type_(variant_npos) {
    set(std::forward<U>(arg));
  }

  template <class U>
  variant& operator=(U&& arg) {
    set(std::forward<U>(arg));
    return *this;
  }

  variant(const variant& other) : type_(variant_npos) {
    variant_assign_helper<variant> helper{*this};
    other.template apply<void>(helper);
  }

  variant(variant&& other) : type_(variant_npos) {
    variant_move_helper<variant> helper{*this};
    other.template apply<void>(helper);
  }

  ~variant() {
    destroy_data();
  }

  constexpr size_t index() const {
    return static_cast<size_t>(type_);
  }

  bool valueless_by_exception() const {
    return index() == variant_npos;
  }

  /// @cond PRIVATE
  template <int Pos>
  bool is(std::integral_constant<int, Pos>) const {
    return type_ == Pos;
  }

  template <class T>
  bool is() const {
    using namespace detail;
    int_token<tl_index_where<type_list<Ts...>,
                             tbind<is_same_ish, T>::template type>::value>
      token;
    return is(token);
  }

  template <int Pos>
  const typename detail::tl_at<types, Pos>::type&
  get(std::integral_constant<int, Pos> token) const {
    return data_.get(token);
  }

  template <int Pos>
  typename detail::tl_at<types, Pos>::type&
  get(std::integral_constant<int, Pos> token) {
    return data_.get(token);
  }

  template <class Result, class Visitor, class... Variants>
  Result apply(Visitor&& visitor, Variants&&... xs) const {
    return apply_impl<Result>(*this, std::forward<Visitor>(visitor),
                              std::forward<Variants>(xs)..., variant_marker);
  }

  template <class Result, class Visitor, class... Variants>
  Result apply(Visitor&& visitor, Variants&&... xs) {
    return apply_impl<Result>(*this, std::forward<Visitor>(visitor),
                              std::forward<Variants>(xs)..., variant_marker);
  }

  template <class Result, class Self, class Visitor, class... Us>
  static Result apply_impl(Self& x, Visitor&& f, variant_marker_t, Us&&... xs) {
    switch (x.type_) {
      default: CAF_RAISE_ERROR("invalid type found");
      CAF_VARIANT_CASE(0);
      CAF_VARIANT_CASE(1);
      CAF_VARIANT_CASE(2);
      CAF_VARIANT_CASE(3);
      CAF_VARIANT_CASE(4);
      CAF_VARIANT_CASE(5);
      CAF_VARIANT_CASE(6);
      CAF_VARIANT_CASE(7);
      CAF_VARIANT_CASE(8);
      CAF_VARIANT_CASE(9);
      CAF_VARIANT_CASE(10);
      CAF_VARIANT_CASE(11);
      CAF_VARIANT_CASE(12);
      CAF_VARIANT_CASE(13);
      CAF_VARIANT_CASE(14);
      CAF_VARIANT_CASE(15);
      CAF_VARIANT_CASE(16);
      CAF_VARIANT_CASE(17);
      CAF_VARIANT_CASE(18);
      CAF_VARIANT_CASE(19);
    }
  }

  template <class Result, class Self, class Visitor, class Variant, class... Us>
  static detail::enable_if_t<is_variant<Variant>::value, Result>
  apply_impl(Self& x, Visitor&& f, Variant&& next, Us&&... xs) {
    switch (x.type_) {
      default: CAF_RAISE_ERROR("invalid type found");
      CAF_VARIANT_DISPATCH_CASE(0);
      CAF_VARIANT_DISPATCH_CASE(1);
      CAF_VARIANT_DISPATCH_CASE(2);
      CAF_VARIANT_DISPATCH_CASE(3);
      CAF_VARIANT_DISPATCH_CASE(4);
      CAF_VARIANT_DISPATCH_CASE(5);
      CAF_VARIANT_DISPATCH_CASE(6);
      CAF_VARIANT_DISPATCH_CASE(7);
      CAF_VARIANT_DISPATCH_CASE(8);
      CAF_VARIANT_DISPATCH_CASE(9);
      CAF_VARIANT_DISPATCH_CASE(10);
      CAF_VARIANT_DISPATCH_CASE(11);
      CAF_VARIANT_DISPATCH_CASE(12);
      CAF_VARIANT_DISPATCH_CASE(13);
      CAF_VARIANT_DISPATCH_CASE(14);
      CAF_VARIANT_DISPATCH_CASE(15);
      CAF_VARIANT_DISPATCH_CASE(16);
      CAF_VARIANT_DISPATCH_CASE(17);
      CAF_VARIANT_DISPATCH_CASE(18);
      CAF_VARIANT_DISPATCH_CASE(19);
    }
  }
  /// @endcond

private:
  inline void destroy_data() {
    if (type_ == variant_npos) return; // nothing to do
    detail::variant_data_destructor f;
    apply<void>(f);
  }

  template <class U>
  void set(U&& arg) {
    using type = typename std::decay<U>::type;
    static constexpr int type_id =
      detail::tl_index_where<
        types,
        detail::tbind<is_same_ish, type>::template type
      >::value;
    static_assert(type_id >= 0, "invalid type for variant");
    std::integral_constant<int, type_id> token;
    if (type_ != type_id) {
      destroy_data();
      type_ = type_id;
      auto& ref = data_.get(token);
      new (&ref) type (std::forward<U>(arg));
    } else {
       data_.get(token) = std::forward<U>(arg);
    }
  }

  template <class... Us>
  void set(const variant<Us...>& other) {
    using namespace detail;
    static_assert(tl_subset_of<type_list<Us...>, types>::value,
                  "cannot set variant of type A to variant of type B "
                  "unless the element types of A are a strict subset of "
                  "the element types of B");
    variant_assign_helper<variant> helper{*this};
    other.template apply<void>(helper);
  }

  template <class... Us>
  void set(variant<Us...>& other) {
    set(const_cast<const variant<Us...>&>(other));
  }

  template <class... Us>
  void set(variant<Us...>&& other) {
    using namespace detail;
    static_assert(tl_subset_of<type_list<Us...>, types>::value,
                  "cannot set variant of type A to variant of type B "
                  "unless the element types of A are a strict subset of "
                  "the element types of B");
    variant_move_helper<variant> helper{*this};
    other.template apply<void>(helper);
  }

  size_t type_;
  detail::variant_data<typename lift_void<Ts>::type...> data_;
};

/// @relates variant
template <class T, class... Us>
T& get(variant<Us...>& value) {
  using namespace detail;
  int_token<tl_index_where<type_list<Us...>,
                           tbind<is_same_ish, T>::template type>::value> token;
  // silence compiler error about "binding to unrelated types" such as
  // 'signed char' to 'char' (which is obvious bullshit)
  return reinterpret_cast<T&>(value.get(token));
}

/// @relates variant
template <class T, class... Us>
const T& get(const variant<Us...>& value) {
  // compiler implicitly restores const because of the return type
  return get<T>(const_cast<variant<Us...>&>(value));
}

/// @relates variant
template <class T, class... Us>
T* get_if(variant<Us...>* value) {
  using namespace detail;
  int_token<tl_index_where<type_list<Us...>,
                           tbind<is_same_ish, T>::template type>::value> token;
  if (value->is(token))
    return &get<T>(*value);
  return nullptr;
}

/// @relates variant
template <class T, class... Us>
const T* get_if(const variant<Us...>* value) {
  // compiler implicitly restores const because of the return type
  return get_if<T>(const_cast<variant<Us...>*>(value));
}

/// @relates variant
template <class Visitor, class Variant, class... Variants,
          class Result = variant_visit_result_t<Visitor, Variant, Variants...>>
Result visit(Visitor&& f, Variant&& x, Variants&&... xs) {
  return x.template apply<Result>(std::forward<Visitor>(f),
                                  std::forward<Variants>(xs)...);
}

template <class T, class... Ts>
bool holds_alternative(const variant<Ts...>& data) {
  return data.template is<T>();
}

/// @relates variant
template <class T>
struct variant_compare_helper {
  using result_type = bool;
  const T& lhs;
  variant_compare_helper(const T& lhs_ref) : lhs(lhs_ref) {
    // nop
  }
  template <class U>
  bool operator()(const U& rhs) const {
    auto ptr = get_if<U>(&lhs);
    return ptr ? *ptr == rhs : false;
  }
};

/// @relates variant
template <class... Ts>
bool operator==(const variant<Ts...>& x, const variant<Ts...>& y) {
  variant_compare_helper<variant<Ts...>> f{x};
  return visit(f, y);
}

/// @relates variant
template <class T, class... Ts>
bool operator==(const T& x, const variant<Ts...>& y) {
  variant_compare_helper<variant<Ts...>> f{y};
  return f(x);
}

/// @relates variant
template <class T, class... Ts>
bool operator==(const variant<Ts...>& x, const T& y) {
  return y == x;
}

/// @relates variant
template <class T>
struct variant_reader {
  size_t& type_tag;
  T& x;
};

/// @relates variant
template <class Inspector, class... Ts>
typename Inspector::result_type
inspect(Inspector& f, variant_reader<variant<Ts...>>& x) {
  return x.x.template apply<typename Inspector::result_type>(f);
}

/// @relates variant
template <class Inspector, class... Ts>
typename std::enable_if<Inspector::reads_state,
                        typename Inspector::result_type>::type
inspect(Inspector& f, variant<Ts...>& x) {
  auto type_tag = x.index();
  variant_reader<variant<Ts...>> helper{type_tag, x};
  return f(meta::omittable(), type_tag, helper);
}

/// @relates variant
template <class T>
struct variant_writer {
  size_t& type_tag;
  T& x;
};

/// @relates variant
template <class Inspector, class... Ts>
typename Inspector::result_type
inspect(Inspector& f, variant_writer<variant<Ts...>>& x) {
  switch (x.type_tag) {
    default: CAF_RAISE_ERROR("invalid type found");
    CAF_VARIANT_ASSIGN_CASE(0);
    CAF_VARIANT_ASSIGN_CASE(1);
    CAF_VARIANT_ASSIGN_CASE(2);
    CAF_VARIANT_ASSIGN_CASE(3);
    CAF_VARIANT_ASSIGN_CASE(4);
    CAF_VARIANT_ASSIGN_CASE(5);
    CAF_VARIANT_ASSIGN_CASE(6);
    CAF_VARIANT_ASSIGN_CASE(7);
    CAF_VARIANT_ASSIGN_CASE(8);
    CAF_VARIANT_ASSIGN_CASE(9);
    CAF_VARIANT_ASSIGN_CASE(10);
    CAF_VARIANT_ASSIGN_CASE(11);
    CAF_VARIANT_ASSIGN_CASE(12);
    CAF_VARIANT_ASSIGN_CASE(13);
    CAF_VARIANT_ASSIGN_CASE(14);
    CAF_VARIANT_ASSIGN_CASE(15);
    CAF_VARIANT_ASSIGN_CASE(16);
    CAF_VARIANT_ASSIGN_CASE(17);
    CAF_VARIANT_ASSIGN_CASE(18);
    CAF_VARIANT_ASSIGN_CASE(19);
  }
}

/// @relates variant
template <class Inspector, class... Ts>
typename std::enable_if<Inspector::writes_state,
                        typename Inspector::result_type>::type
inspect(Inspector& f, variant<Ts...>& x) {
  size_t type_tag;
  variant_writer<variant<Ts...>> helper{type_tag, x};
  return f(meta::omittable(), type_tag, helper);
}

} // namespace caf

#endif // CAF_VARIANT_HPP
