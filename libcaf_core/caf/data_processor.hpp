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

#ifndef CAF_DATA_PROCESSOR_HPP
#define CAF_DATA_PROCESSOR_HPP

#include <chrono>
#include <string>
#include <cstdint>
#include <cstddef> // size_t
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/atom.hpp"
#include "caf/error.hpp"
#include "caf/allowed_unsafe_message_type.hpp"

#include "caf/meta/annotation.hpp"
#include "caf/meta/save_callback.hpp"
#include "caf/meta/load_callback.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/delegate_serialize.hpp"
#include "caf/detail/select_integer_type.hpp"

namespace caf {


/// A data processor translates an object into a format that can be
/// stored or vice versa. A data processor can be either in saving
/// or loading mode.
template <class Derived>
class data_processor {
public:
  data_processor(const data_processor&) = delete;
  data_processor& operator=(const data_processor&) = delete;

  data_processor(execution_unit* ctx = nullptr) : context_(ctx) {
    // nop
  }

  virtual ~data_processor() {
    // nop
  }

  /// Begins processing of an object. Saves the type information
  /// to the underlying storage when in saving mode, otherwise
  /// extracts them and sets both arguments accordingly.
  virtual error begin_object(uint16_t& typenr, std::string& name) = 0;

  /// Ends processing of an object.
  virtual error end_object() = 0;

  /// Begins processing of a sequence. Saves the size
  /// to the underlying storage when in saving mode, otherwise
  /// sets `num` accordingly.
  virtual error begin_sequence(size_t& num) = 0;

  /// Ends processing of a sequence.
  virtual error end_sequence() = 0;

  /// Returns the actor system associated to this data processor.
  execution_unit* context() {
    return context_;
  }

  using builtin_t =
    detail::type_list<
      int8_t,
      uint8_t,
      int16_t,
      uint16_t,
      int32_t,
      uint32_t,
      int64_t,
      uint64_t,
      float,
      double,
      long double,
      std::string,
      std::u16string,
      std::u32string
    >;

  /// Lists all types an implementation has to accept as builtin types.
  enum builtin {
    i8_v,
    u8_v,
    i16_v,
    u16_v,
    i32_v,
    u32_v,
    i64_v,
    u64_v,
    float_v,
    double_v,
    ldouble_v,
    string8_v,
    string16_v,
    string32_v
  };

  /// Applies this processor to an arithmetic type.
  template <class T>
  typename std::enable_if<std::is_floating_point<T>::value, error>::type
  apply(T& x) {
    static constexpr auto tlindex = detail::tl_index_of<builtin_t, T>::value;
    static_assert(tlindex >= 0, "T not recognized as builtiln type");
    return apply_builtin(static_cast<builtin>(tlindex), &x);
  }

  template <class T>
  typename std::enable_if<
    std::is_integral<T>::value
    && ! std::is_same<bool, T>::value,
    error
  >::type
  apply(T& x) {
    using type =
      typename detail::select_integer_type<
        static_cast<int>(sizeof(T)) * (std::is_signed<T>::value ? -1 : 1)
      >::type;
    static constexpr auto tlindex = detail::tl_index_of<builtin_t, type>::value;
    static_assert(tlindex >= 0, "T not recognized as builtiln type");
    return apply_builtin(static_cast<builtin>(tlindex), &x);
  }

  error apply(std::string& x) {
    return apply_builtin(string8_v, &x);
  }

  error apply(std::u16string& x) {
    return apply_builtin(string16_v, &x);
  }

  error apply(std::u32string& x) {
    return apply_builtin(string32_v, &x);
  }

  template <class D, atom_value V>
  static error apply_atom_constant(D& self, atom_constant<V>&) {
    static_assert(D::is_saving::value, "cannot deserialize an atom_constant");
    auto x = V;
    return self.apply(x);
  }

  template <atom_value V>
  error apply(atom_constant<V>& x) {
    return apply_atom_constant(dref(), x);
  }

  /// Serializes enums using the underlying type
  /// if no user-defined serialization is defined.
  template <class T>
  typename std::enable_if<
    std::is_enum<T>::value
    && ! detail::has_serialize<T>::value,
    error
  >::type
  apply(T& x) {
    using underlying = typename std::underlying_type<T>::type;
    struct {
      void operator()(T& lhs, underlying& rhs) const {
        lhs = static_cast<T>(rhs);
      }
      void operator()(underlying& lhs, T& rhs) const {
        lhs = static_cast<underlying>(rhs);
      }
    } assign;
    underlying tmp;
    return convert_apply(dref(), x, tmp, assign);
  }

  /// Applies this processor to an empty type.
  template <class T>
  typename std::enable_if<std::is_empty<T>::value, error>::type
  apply(T&) {
    return none;
  }

  error apply(bool& x) {
    struct {
      void operator()(bool& lhs, uint8_t& rhs) const {
        lhs = rhs != 0;
      }
      void operator()(uint8_t& lhs, bool& rhs) const {
        lhs = rhs ? 1 : 0;
      }
    } assign;
    uint8_t tmp;
    return convert_apply(dref(), x, tmp, assign);
  }

  template <class T>
  error consume_range(T& xs) {
    for (auto& x : xs) {
      using value_type = typename std::remove_const<
                           typename std::remove_reference<decltype(x)>::type
                         >::type;
      auto e = apply(const_cast<value_type&>(x));
      if (e)
        return e;
    }
    return none;
  }

  /// Converts each element in `xs` to `U` before calling `apply`.
  template <class U, class T>
  error consume_range_c(T& xs) {
    for (U x : xs) {
      auto e = apply(x);
      if (e)
        return e;
    }
    return none;
  }

  template <class T>
  error fill_range(T& xs, size_t num_elements) {
    auto insert_iter = std::inserter(xs, xs.end());
    for (size_t i = 0; i < num_elements; ++i) {
      typename std::remove_const<typename T::value_type>::type x;
      auto err = apply(x);
      if (err)
        return err;
      *insert_iter++ = std::move(x);
    }
    return none;
  }

  /// Loads elements from type `U` before inserting to `xs`.
  template <class U, class T>
  error fill_range_c(T& xs, size_t num_elements) {
    auto insert_iter = std::inserter(xs, xs.end());
    for (size_t i = 0; i < num_elements; ++i) {
      U x;
      auto err = apply(x);
      if (err)
        return err;
      *insert_iter++ = std::move(x);
    }
    return none;
  }
  // Applies this processor as Derived to `xs` in saving mode.
  template <class D, class T>
  static typename std::enable_if<
    D::is_saving::value && ! detail::is_byte_sequence<T>::value,
    error
  >::type
  apply_sequence(D& self, T& xs) {
    auto s = xs.size();
    return error::eval([&] { return self.begin_sequence(s); },
                       [&] { return self.consume_range(xs); },
                       [&] { return self.end_sequence(); });
  }

  // Applies this processor as Derived to `xs` in loading mode.
  template <class D, class T>
  static typename std::enable_if<
    ! D::is_saving::value && ! detail::is_byte_sequence<T>::value,
    error
  >::type
  apply_sequence(D& self, T& xs) {
    size_t s;
    return error::eval([&] { return self.begin_sequence(s); },
                       [&] { return self.fill_range(xs, s); },
                       [&] { return self.end_sequence(); });
  }

  // Optimized saving for contiguous byte sequences.
  template <class D, class T>
  static typename std::enable_if<
    D::is_saving::value && detail::is_byte_sequence<T>::value,
    error
  >::type
  apply_sequence(D& self, T& xs) {
    auto s = xs.size();
    return error::eval([&] { return self.begin_sequence(s); },
                       [&] { return self.apply_raw(xs.size(), &xs[0]); },
                       [&] { return self.end_sequence(); });
  }

  // Optimized loading for contiguous byte sequences.
  template <class D, class T>
  static typename std::enable_if<
    ! D::is_saving::value && detail::is_byte_sequence<T>::value,
    error
  >::type
  apply_sequence(D& self, T& xs) {
    size_t s;
    return error::eval([&] { return self.begin_sequence(s); },
                       [&] { xs.resize(s); return self.apply_raw(s, &xs[0]); },
                       [&] { return self.end_sequence(); });
  }

  /// Applies this processor to a sequence of values.
  template <class T>
  typename std::enable_if<
    detail::is_iterable<T>::value
    && ! detail::has_serialize<T>::value
    && ! detail::is_inspectable<Derived, T>::value,
    error
  >::type
  apply(T& xs) {
    return apply_sequence(dref(), xs);
  }

  /// Applies this processor to an array.
  template <class T, size_t S>
  typename std::enable_if<detail::is_serializable<T>::value, error>::type
  apply(std::array<T, S>& xs) {
    return consume_range(xs);
  }

  /// Applies this processor to an array.
  template <class T, size_t S>
  typename std::enable_if<detail::is_serializable<T>::value, error>::type
  apply(T (&xs) [S]) {
    return consume_range(xs);
  }

  template <class F, class S>
  typename std::enable_if<
    detail::is_serializable<typename std::remove_const<F>::type>::value
    && detail::is_serializable<S>::value,
    error
  >::type
  apply(std::pair<F, S>& xs) {
    using t0 = typename std::remove_const<F>::type;
    // This cast allows the data processor to cope with
    // `pair<const K, V>` value types used by `std::map`.
    return error::eval([&] { return apply(const_cast<t0&>(xs.first)); },
                       [&] { return apply(xs.second); });
  }

  template <class... Ts>
  typename std::enable_if<
    detail::conjunction<
      detail::is_serializable<Ts>::value...
    >::value,
    error
  >::type
  apply(std::tuple<Ts...>& xs) {
    //apply_helper f{*this};
    return detail::apply_args(*this, detail::get_indices(xs), xs);
  }

  template <class T>
  typename std::enable_if<
    ! std::is_empty<T>::value
    && detail::has_serialize<T>::value,
    error
  >::type
  apply(T& x) {
    // throws on error
    detail::delegate_serialize(dref(), x);
    return none;
  }

  template <class Rep, class Period>
  typename std::enable_if<std::is_integral<Rep>::value, error>::type
  apply(std::chrono::duration<Rep, Period>& x) {
    using duration_type = std::chrono::duration<Rep, Period>;
    // always save/store durations as int64_t to work around possibly
    // different integer types on different plattforms for standard typedefs
    struct {
      void operator()(duration_type& lhs, int64_t& rhs) const {
        duration_type tmp{rhs};
        lhs = tmp;
      }
      void operator()(int64_t& lhs, duration_type& rhs) const {
        lhs = rhs.count();
      }
    } assign;
    int64_t tmp;
    return convert_apply(dref(), x, tmp, assign);
  }

  template <class Rep, class Period>
  typename std::enable_if<std::is_floating_point<Rep>::value, error>::type
  apply(std::chrono::duration<Rep, Period>& x) {
    using duration_type = std::chrono::duration<Rep, Period>;
    // always save/store floating point durations
    // as doubles for the same reason as above
    struct {
      void operator()(duration_type& lhs, double& rhs) const {
        duration_type tmp{rhs};
        lhs = tmp;
      }
      void operator()(double& lhs, duration_type& rhs) const {
        lhs = rhs.count();
      }
    } assign;
    double tmp;
    return convert_apply(dref(), x, tmp, assign);
  }

  /// Applies this processor to a raw block of data of size `num_bytes`.
  virtual error apply_raw(size_t num_bytes, void* data) = 0;

  template <class T>
  auto apply(T& x) -> decltype(inspect(std::declval<Derived&>(), x)) {
    return inspect(dref(), x);
  }

  inline error operator()() {
    return none;
  }

  template <class F, class... Ts>
  error operator()(meta::save_callback_t<F> x, Ts&&... xs) {
    error e;
    if (Derived::is_saving::value)
      e = x.fun();
    return e ? e : (*this)(std::forward<Ts>(xs)...);
  }

  template <class F, class... Ts>
  error operator()(meta::load_callback_t<F> x, Ts&&... xs) {
    error e;
    if (Derived::is_loading::value)
      e = x.fun();
    return e ? e : (*this)(std::forward<Ts>(xs)...);
  }

  template <class... Ts>
  error operator()(const meta::annotation&, Ts&&... xs) {
    return (*this)(std::forward<Ts>(xs)...);
  }

  template <class T, class... Ts>
  typename std::enable_if<
    allowed_unsafe_message_type<T>::value,
    error
  >::type
  operator()(T&, Ts&&... xs) {
    return (*this)(std::forward<Ts>(xs)...);
  }

  template <class T, class... Ts>
  typename std::enable_if<
    ! meta::is_annotation<T>::value
    && ! allowed_unsafe_message_type<T>::value,
    error
  >::type
  operator()(T& x, Ts&&... xs) {
    auto e = apply(x);
    return e ? e : (*this)(std::forward<Ts>(xs)...);
  }

protected:
  /// Applies this processor to a single builtin value.
  virtual error apply_builtin(builtin in_out_type, void* in_out) = 0;

private:
  template <class D, class T, class U, class F>
  static typename std::enable_if<D::is_saving::value, error>::type
  convert_apply(D& self, T& x, U& storage, F assign) {
    assign(storage, x);
    return self.apply(storage);
  }

  template <class D, class T, class U, class F>
  static typename std::enable_if<! D::is_saving::value, error>::type
  convert_apply(D& self, T& x, U& storage, F assign) {
    auto e = self.apply(storage);
    if (e)
      return e;
    assign(x, storage);
    return none;
  }

  // Returns a reference to the derived type.
  Derived& dref() {
    return *static_cast<Derived*>(this);
  }

  execution_unit* context_;
};

} // namespace caf

#endif // CAF_DATA_PROCESSOR_HPP
