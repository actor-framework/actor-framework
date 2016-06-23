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
  using streambuf_type = typename std::remove_reference<Streambuf>::type;
  using char_type = typename streambuf_type::char_type;
  using streambuf_base = std::basic_streambuf<char_type>;
  static_assert(std::is_base_of<streambuf_base, streambuf_type>::value,
                "Streambuf must inherit from std::streambuf");

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
    varbyte_decode(num_elements);
  }

  void end_sequence() override {
    // nop
  }

  void apply_raw(size_t num_bytes, void* data) override {
    auto n = streambuf_.sgetn(reinterpret_cast<char_type*>(data),
                              static_cast<std::streamsize>(num_bytes));
    CAF_ASSERT(n >= 0);
    range_check(static_cast<size_t>(n), num_bytes);
  }

protected:
  // Decode an unsigned integral type as variable-byte-encoded byte sequence.
  template <class T>
  size_t varbyte_decode(T& x) {
    static_assert(std::is_unsigned<T>::value, "T must be an unsigned type");
    auto n = T{0};
    x = 0;
    uint8_t low7;
    do {
       auto c = streambuf_.sbumpc();
       using traits = typename streambuf_type::traits_type;
       if (traits::eq_int_type(c, traits::eof()))
         CAF_RAISE_ERROR("stream_deserializer<T>::begin_sequence");
      low7 = static_cast<uint8_t>(traits::to_char_type(c));
      x |= static_cast<T>((low7 & 0x7F) << (7 * n));
      ++n;
    } while (low7 & 0x80);
    return n;
  }

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
        size_t str_size;
        begin_sequence(str_size);
        str.resize(str_size);
        // TODO: When using C++14, switch to str.data(), which then has a
        // non-const overload.
        auto data = reinterpret_cast<char_type*>(&str[0]);
        auto n = streambuf_.sgetn(data, static_cast<std::streamsize>(str_size));
        CAF_ASSERT(n >= 0);
        range_check(static_cast<size_t>(n), str_size);
        end_sequence();
        break;
      }
      case string16_v: {
        auto& str = *reinterpret_cast<std::u16string*>(val);
        size_t str_size;
        begin_sequence(str_size);
        auto bytes = str_size * sizeof(std::u16string::value_type);
        str.resize(str_size);
        // TODO: When using C++14, switch to str.data(), which then has a
        // non-const overload.
        auto data = reinterpret_cast<char_type*>(&str[0]);
        auto n = streambuf_.sgetn(data, static_cast<std::streamsize>(bytes));
        CAF_ASSERT(n >= 0);
        range_check(static_cast<size_t>(n), str_size);
        end_sequence();
        break;
      }
      case string32_v: {
        auto& str = *reinterpret_cast<std::u32string*>(val);
        size_t str_size;
        begin_sequence(str_size);
        auto bytes = str_size * sizeof(std::u32string::value_type);
        str.resize(str_size);
        // TODO: When using C++14, switch to str.data(), which then has a
        // non-const overload.
        auto data = reinterpret_cast<char_type*>(&str[0]);
        auto n = streambuf_.sgetn(data, static_cast<std::streamsize>(bytes));
        CAF_ASSERT(n >= 0);
        range_check(static_cast<size_t>(n), str_size);
        end_sequence();
        break;
      }
    }
  }

private:
  void range_check(size_t got, size_t need) {
    if (got != need) {
      CAF_LOG_ERROR("range_check failed");
      CAF_RAISE_ERROR("stream_deserializer<T>::range_check()");
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
