/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
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

#include "caf/static_visitor.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/variant_data.hpp"

#define CAF_VARIANT_CASE(x)                                                    \
  case x:                                                                      \
    return visitor(from.get(                                                   \
      std::integral_constant<int, (x < max_type_id ? x : max_type_id)>()))

namespace caf {

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

template <class T, class U,
          bool Enable = std::is_integral<T>::value
                        && std::is_integral<U>::value
                        && ! std::is_same<T, bool>::value>
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

/// A variant represents always a valid value of one of the types `Ts...`.
template <class... Ts>
class variant {
public:
  using types = detail::type_list<Ts...>;

  static constexpr int max_type_id = sizeof...(Ts) - 1;

  static_assert(! detail::tl_exists<types, std::is_reference>::value,
                "Cannot create a variant of references");

  variant& operator=(const variant& other) {
    variant_assign_helper<variant> helper{*this};
    other.apply(helper);
    return *this;
  }

  variant& operator=(variant&& other) {
    variant_move_helper<variant> helper{*this};
    other.apply(helper);
    return *this;
  }

  variant() : type_(0) {
    // never empty
    set(typename detail::tl_head<types>::type());
  }

  template <class U>
  variant(U&& arg) : type_(-1) {
    set(std::forward<U>(arg));
  }

  template <class U>
  variant& operator=(U&& arg) {
    set(std::forward<U>(arg));
    return *this;
  }

  variant(const variant& other) : type_(-1) {
    variant_assign_helper<variant> helper{*this};
    other.apply(helper);
  }

  variant(variant&& other) : type_(-1) {
    variant_move_helper<variant> helper{*this};
    other.apply(helper);
  }

  ~variant() {
    destroy_data();
  }

  /// @cond PRIVATE
  template <int Pos>
  bool is(std::integral_constant<int, Pos>) const {
    return type_ == Pos;
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

  template <class Visitor>
  typename Visitor::result_type apply(Visitor& visitor) const {
    return apply_impl(*this, visitor);
  }

  template <class Visitor>
  typename Visitor::result_type apply(Visitor& visitor) {
    return apply_impl(*this, visitor);
  }
  /// @endcond

private:
  template <class Self, typename Visitor>
  static typename Visitor::result_type  apply_impl(Self& from, Visitor& visitor) {
    switch (from.type_) {
      default: throw std::runtime_error("invalid type found");
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
      CAF_VARIANT_CASE(20);
    }
  }

  inline void destroy_data() {
    if (type_ == -1) return; // nothing to do
    detail::variant_data_destructor f;
    apply(f);
  }

  template <class U>
  void set(U&& arg) {
    using type = typename std::decay<U>::type;
    static constexpr int type_id = detail::tl_find_if<
                       types,
                       detail::tbind<
                         is_same_ish, type
                       >::template type
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
    static_assert(tlf_is_subset<type_list<Us...>, types>(),
                  "cannot set variant of type A to variant of type B "
                  "unless the element types of A are a strict subset of "
                  "the element types of B");
    variant_assign_helper<variant> helper{*this};
    other.apply(helper);
  }

  template <class... Us>
  void set(variant<Us...>&& other) {
    using namespace detail;
    static_assert(tlf_is_subset<type_list<Us...>, types>(),
                  "cannot set variant of type A to variant of type B "
                  "unless the element types of A are a strict subset of "
                  "the element types of B");
    variant_move_helper<variant> helper{*this};
    other.apply(helper);
  }

  int type_;
  detail::variant_data<typename lift_void<Ts>::type...> data_;
};

/// @relates variant
template <class T, class... Us>
T& get(variant<Us...>& value) {
  using namespace detail;
  int_token<tl_find_if<type_list<Us...>,
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
T* get(variant<Us...>* value) {
  using namespace detail;
  int_token<tl_find_if<type_list<Us...>,
                       tbind<is_same_ish, T>::template type>::value> token;
  if (value->is(token))
    return &get<T>(*value);
  return nullptr;
}

/// @relates variant
template <class T, class... Us>
const T* get(const variant<Us...>* value) {
  // compiler implicitly restores const because of the return type
  return get<T>(const_cast<variant<Us...>*>(value));
}

/// @relates variant
template <class Visitor, class... Ts>
typename Visitor::result_type apply_visitor(Visitor& visitor,
                      const variant<Ts...>& data) {
  return data.apply(visitor);
}

/// @relates variant
template <class Visitor, class... Ts>
typename Visitor::result_type apply_visitor(Visitor& visitor,
                      variant<Ts...>& data) {
  return data.apply(visitor);
}

} // namespace caf

#endif // CAF_VARIANT_HPP
