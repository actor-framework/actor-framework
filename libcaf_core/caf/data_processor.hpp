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
  virtual void begin_object(uint16_t& typenr, std::string& name) = 0;

  /// Ends processing of an object.
  virtual void end_object() = 0;

  /// Begins processing of a sequence. Saves the size
  /// to the underlying storage when in saving mode, otherwise
  /// sets `num` accordingly.
  virtual void begin_sequence(size_t& num) = 0;

  /// Ends processing of a sequence.
  virtual void end_sequence() = 0;

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
  typename std::enable_if<std::is_floating_point<T>::value>::type
  apply(T& x) {
    static constexpr auto tlindex = detail::tl_index_of<builtin_t, T>::value;
    static_assert(tlindex >= 0, "T not recognized as builtiln type");
    apply_builtin(static_cast<builtin>(tlindex), &x);
  }

  template <class T>
  typename std::enable_if<
    std::is_integral<T>::value
    && ! std::is_same<bool, T>::value
  >::type
  apply(T& x) {
    using type =
      typename detail::select_integer_type<
        static_cast<int>(sizeof(T)) * (std::is_signed<T>::value ? -1 : 1)
      >::type;
    static constexpr auto tlindex = detail::tl_index_of<builtin_t, type>::value;
    static_assert(tlindex >= 0, "T not recognized as builtiln type");
    apply_builtin(static_cast<builtin>(tlindex), &x);
  }

  void apply(std::string& x) {
    apply_builtin(string8_v, &x);
  }

  void apply(std::u16string& x) {
    apply_builtin(string16_v, &x);
  }

  void apply(std::u32string& x) {
    apply_builtin(string32_v, &x);
  }

  template <class D, atom_value V>
  static void apply_atom_constant(D& self, atom_constant<V>&) {
    static_assert(D::is_saving::value, "cannot deserialize an atom_constant");
    auto x = V;
    self.apply(x);
  }

  template <atom_value V>
  void apply(atom_constant<V>& x) {
    apply_atom_constant(dref(), x);
  }

  /// Serializes enums using the underlying type
  /// if no user-defined serialization is defined.
  template <class T>
  typename std::enable_if<
    std::is_enum<T>::value
    && ! detail::has_serialize<T>::value
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
    convert_apply(dref(), x, tmp, assign);
  }

  /// Applies this processor to an empty type.
  template <class T>
  typename std::enable_if<std::is_empty<T>::value>::type
  apply(T&) {
    // nop
  }

  void apply(bool& x) {
    struct {
      void operator()(bool& lhs, uint8_t& rhs) const {
        lhs = rhs != 0;
      }
      void operator()(uint8_t& lhs, bool& rhs) const {
        lhs = rhs ? 1 : 0;
      }
    } assign;
    uint8_t tmp;
    convert_apply(dref(), x, tmp, assign);
  }

  // Applies this processor as Derived to `xs` in saving mode.
  template <class D, class T>
  static typename std::enable_if<
    D::is_saving::value && ! detail::is_byte_sequence<T>::value
  >::type
  apply_sequence(D& self, T& xs) {
    auto s = xs.size();
    self.begin_sequence(s);
    using value_type = typename std::remove_const<typename T::value_type>::type;
    for (auto& x : xs)
      self.apply(const_cast<value_type&>(x));
    self.end_sequence();
  }

  // Applies this processor as Derived to `xs` in loading mode.
  template <class D, class T>
  static typename std::enable_if<
    ! D::is_saving::value && ! detail::is_byte_sequence<T>::value
  >::type
  apply_sequence(D& self, T& xs) {
    size_t num_elements;
    self.begin_sequence(num_elements);
    auto insert_iter = std::inserter(xs, xs.end());
    for (size_t i = 0; i < num_elements; ++i) {
      typename std::remove_const<typename T::value_type>::type x;
      self.apply(x);
      *insert_iter++ = std::move(x);
    }
    self.end_sequence();
  }

  // Optimized saving for contiguous byte sequences.
  template <class D, class T>
  static typename std::enable_if<
    D::is_saving::value && detail::is_byte_sequence<T>::value
  >::type
  apply_sequence(D& self, T& xs) {
    auto s = xs.size();
    self.begin_sequence(s);
    self.apply_raw(xs.size(), &xs[0]);
    self.end_sequence();
  }

  // Optimized loading for contiguous byte sequences.
  template <class D, class T>
  static typename std::enable_if<
    ! D::is_saving::value && detail::is_byte_sequence<T>::value
  >::type
  apply_sequence(D& self, T& xs) {
    size_t num_elements;
    self.begin_sequence(num_elements);
    xs.resize(num_elements);
    self.apply_raw(xs.size(), &xs[0]);
    self.end_sequence();
  }

  /// Applies this processor to a sequence of values.
  template <class T>
  typename std::enable_if<
    detail::is_iterable<T>::value
    && ! detail::has_serialize<T>::value
  >::type
  apply(T& xs) {
    apply_sequence(dref(), xs);
  }

  /// Applies this processor to an array.
  template <class T, size_t S>
  typename std::enable_if<detail::is_serializable<T>::value>::type
  apply(std::array<T, S>& xs) {
    for (auto& x : xs)
      apply(x);
  }

  /// Applies this processor to an array.
  template <class T, size_t S>
  typename std::enable_if<detail::is_serializable<T>::value>::type
  apply(T (&xs) [S]) {
    for (auto& x : xs)
      apply(x);
  }

  template <class F, class S>
  typename std::enable_if<
    detail::is_serializable<typename std::remove_const<F>::type>::value
    && detail::is_serializable<S>::value
  >::type
  apply(std::pair<F, S>& xs) {
    // This cast allows the data processor to cope with
    // `pair<const K, V>` value types used by `std::map`.
    apply(const_cast<typename std::remove_const<F>::type&>(xs.first));
    apply(xs.second);
  }

  class apply_helper {
  public:
    data_processor& parent;

    apply_helper(data_processor& dp) : parent(dp) {
      // nop
    }

    inline void operator()() {
      // end of recursion
    }

    template <class T, class... Ts>
    void operator()(T& x, Ts&... xs) {
      parent.apply(x);
      (*this)(xs...);
    }
  };

  template <class... Ts>
  typename std::enable_if<
    detail::conjunction<
      detail::is_serializable<Ts>::value...
    >::value
  >::type
  apply(std::tuple<Ts...>& xs) {
    apply_helper f{*this};
    detail::apply_args(f, detail::get_indices(xs), xs);
  }

  template <class T>
  typename std::enable_if<
    ! std::is_empty<T>::value
    && detail::has_serialize<T>::value
  >::type
  apply(T& x) {
    detail::delegate_serialize(dref(), x);
  }

  template <class Rep, class Period>
  typename std::enable_if<
    std::is_integral<Rep>::value
  >::type
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
    convert_apply(dref(), x, tmp, assign);
  }

  template <class Rep, class Period>
  typename std::enable_if<
    std::is_floating_point<Rep>::value
  >::type
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
    convert_apply(dref(), x, tmp, assign);
  }

  /// Applies this processor to a raw block of data of size `num_bytes`.
  virtual void apply_raw(size_t num_bytes, void* data) = 0;

protected:
  /// Applies this processor to a single builtin value.
  virtual void apply_builtin(builtin in_out_type, void* in_out) = 0;

private:
  template <class D, class T, class U, class F>
  static typename std::enable_if<D::is_saving::value>::type
  convert_apply(D& self, T& x, U& storage, F assign) {
    assign(storage, x);
    self.apply(storage);
  }

  template <class D, class T, class U, class F>
  static typename std::enable_if<! D::is_saving::value>::type
  convert_apply(D& self, T& x, U& storage, F assign) {
    self.apply(storage);
    assign(x, storage);
  }

  // Returns a reference to the derived type.
  Derived& dref() {
    return *static_cast<Derived*>(this);
  }

  execution_unit* context_;
};

} // namespace caf

#endif // CAF_DATA_PROCESSOR_HPP
