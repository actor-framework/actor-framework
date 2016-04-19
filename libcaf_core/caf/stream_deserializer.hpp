/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_STREAM_DESERIALIZER_HPP
#define CAF_STREAM_DESERIALIZER_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "caf/deserializer.hpp"
#include "caf/logger.hpp"

#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"

namespace caf {

/// Implements the deserializer interface with a binary serialization protocol.
template <class Streambuf>
class stream_deserializer : public deserializer {
  static_assert(
    std::is_base_of<
      std::streambuf,
      typename std::remove_reference<Streambuf>::type
    >::value,
    "Streambuf must inherit from std::streambuf"
  );

public:
  template <class... Ts>
  explicit stream_deserializer(actor_system& sys, Ts&&... xs)
    : deserializer(sys),
      streambuf_(std::forward<Ts>(xs)...) {
  }

  template <class... Ts>
  explicit stream_deserializer(execution_unit* ctx, Ts&&... xs)
    : deserializer(ctx),
      streambuf_(std::forward<Ts>(xs)...) {
  }

  template <
    class S,
    class = typename std::enable_if<
      std::is_same<
        typename std::remove_reference<S>::type,
        typename std::remove_reference<Streambuf>::type
      >::value
    >::type
  >
  explicit stream_deserializer(S&& sb)
    : deserializer(nullptr),
      streambuf_(std::forward<S>(sb)) {
  }

  void begin_object(uint16_t& typenr, std::string& name) override {
    apply_int(typenr);
    if (typenr == 0)
      apply(name);
  }

  void end_object() override {
    // nop
  }

  void begin_sequence(size_t& num_elements) override {
    static_assert(sizeof(size_t) >= sizeof(uint32_t),
                  "sizeof(size_t) < sizeof(uint32_t)");
    uint32_t tmp;
    apply_int(tmp);
    num_elements = static_cast<size_t>(tmp);
  }

  void end_sequence() override {
    // nop
  }

  void apply_raw(size_t num_bytes, void* data) override {
    range_check(num_bytes);
    streambuf_.sgetn(reinterpret_cast<char*>(data), num_bytes);
  }

protected:
  void apply_builtin(builtin type, void* val) override {
    CAF_ASSERT(val != nullptr);
    switch (type) {
      case i8_v:
      case u8_v:
        apply_raw(sizeof(uint8_t), val);
        break;
      case i16_v:
      case u16_v:
        apply_int(*reinterpret_cast<uint16_t*>(val));
        break;
      case i32_v:
      case u32_v:
        apply_int(*reinterpret_cast<uint32_t*>(val));
        break;
      case i64_v:
      case u64_v:
        apply_int(*reinterpret_cast<uint64_t*>(val));
        break;
      case float_v:
        apply_float(*reinterpret_cast<float*>(val));
        break;
      case double_v:
        apply_float(*reinterpret_cast<double*>(val));
        break;
      case ldouble_v: {
        // the IEEE-754 conversion does not work for long double
        // => fall back to string serialization (even though it sucks)
        std::string tmp;
        apply(tmp);
        std::istringstream iss{std::move(tmp)};
        iss >> *reinterpret_cast<long double*>(val);
        break;
      }
      case string8_v: {
        auto& str = *reinterpret_cast<std::string*>(val);
        uint32_t str_size;
        apply_int(str_size);
        range_check(str_size);
        str.resize(str_size);
        // TODO: When using C++14, switch to str.data(), which then has a
        // non-const overload.
        streambuf_.sgetn(reinterpret_cast<char*>(&str[0]), str_size);
        break;
      }
      case string16_v: {
        auto& str = *reinterpret_cast<std::u16string*>(val);
        uint32_t str_size;
        apply_int(str_size);
        auto bytes = str_size * sizeof(std::u16string::value_type);
        range_check(bytes);
        str.resize(str_size);
        // TODO: When using C++14, switch to str.data(), which then has a
        // non-const overload.
        streambuf_.sgetn(reinterpret_cast<char*>(&str[0]), bytes);
        break;
      }
      case string32_v: {
        auto& str = *reinterpret_cast<std::u32string*>(val);
        uint32_t str_size;
        apply_int(str_size);
        auto bytes = str_size * sizeof(std::u32string::value_type);
        range_check(bytes);
        str.resize(str_size);
        // TODO: When using C++14, switch to str.data(), which then has a
        // non-const overload.
        streambuf_.sgetn(reinterpret_cast<char*>(&str[0]), bytes);
        break;
      }
    }
  }

private:
  void range_check(size_t read_size) {
    static_assert(sizeof(std::streamsize) <= sizeof(size_t),
                  "std::streamsize > std::size_t");
    if (static_cast<size_t>(streambuf_.in_avail()) < read_size) {
      CAF_LOG_ERROR("range_check failed");
      throw std::out_of_range("stream_deserializer<T>::read_range()");
    }
  }

  template <class T>
  void apply_int(T& x) {
    T tmp;
    apply_raw(sizeof(T), &tmp);
    x = detail::from_network_order(tmp);
  }

  template <class T>
  void apply_float(T& x) {
    typename detail::ieee_754_trait<T>::packed_type tmp;
    apply_int(tmp);
    x = detail::unpack754(tmp);
  }

  Streambuf streambuf_;
};

} // namespace caf

#endif // CAF_STREAM_DESERIALIZER_HPP
