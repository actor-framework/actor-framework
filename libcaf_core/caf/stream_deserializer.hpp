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

#pragma once

#include <limits>
#include <string>
#include <sstream>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <stdexcept>
#include <type_traits>

#include "caf/sec.hpp"
#include "caf/logger.hpp"
#include "caf/deserializer.hpp"

#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"

namespace caf {

/// Implements the deserializer interface with a binary serialization protocol.
template <class Streambuf>
class stream_deserializer : public deserializer {
  using streambuf_type = typename std::remove_reference<Streambuf>::type;
  using char_type = typename streambuf_type::char_type;
  using streambuf_base = std::basic_streambuf<char_type>;
  using streamsize = std::streamsize;

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

  error begin_object(uint16_t& typenr, std::string& name) override {
    return error::eval([&] { return apply_int(typenr); },
                       [&] { return typenr == 0 ? apply(name) : error{}; });
  }

  error end_object() override {
    return none;
  }

  error begin_sequence(size_t& num_elements) override {
    // We serialize a `size_t` always in 32-bit, to guarantee compatibility
    // with 32-bit nodes in the network.
    // TODO: protect with `if constexpr (sizeof(size_t) > sizeof(uint32_t))`
    //       when switching to C++17 and pass `num_elements` directly to
    //       `varbyte_decode` in the `else` case
    uint32_t x;
    auto result = varbyte_decode(x);
    if (!result)
      num_elements = static_cast<size_t>(x);
    return result;
  }

  error end_sequence() override {
    return none;
  }

  error apply_raw(size_t num_bytes, void* data) override {
    return range_check(streambuf_.sgetn(reinterpret_cast<char_type*>(data),
                                        static_cast<streamsize>(num_bytes)),
                       num_bytes);
  }

protected:
  // Decode an unsigned integral type as variable-byte-encoded byte sequence.
  template <class T>
  error varbyte_decode(T& x) {
    static_assert(std::is_unsigned<T>::value, "T must be an unsigned type");
    auto n = 0;
    x = 0;
    uint8_t low7;
    do {
      auto c = streambuf_.sbumpc();
      using traits = typename streambuf_type::traits_type;
      if (traits::eq_int_type(c, traits::eof()))
        return sec::end_of_stream;
      low7 = static_cast<uint8_t>(traits::to_char_type(c));
      x |= static_cast<T>((low7 & 0x7F)) << (7 * n);
      ++n;
    } while (low7 & 0x80);
    return none;
  }

  error apply_builtin(builtin type, void* val) override {
    CAF_ASSERT(val != nullptr);
    switch (type) {
      default: // i8_v or u8_v
        CAF_ASSERT(type == i8_v || type == u8_v);
        return apply_raw(sizeof(uint8_t), val);
      case i16_v:
      case u16_v:
        return apply_int(*reinterpret_cast<uint16_t*>(val));
      case i32_v:
      case u32_v:
        return apply_int(*reinterpret_cast<uint32_t*>(val));
      case i64_v:
      case u64_v:
        return apply_int(*reinterpret_cast<uint64_t*>(val));
      case float_v:
        return apply_float(*reinterpret_cast<float*>(val));
      case double_v:
        return apply_float(*reinterpret_cast<double*>(val));
      case ldouble_v: {
        // the IEEE-754 conversion does not work for long double
        // => fall back to string serialization (even though it sucks)
        std::string tmp;
        auto e = apply(tmp);
        if (e)
          return e;
        std::istringstream iss{std::move(tmp)};
        iss >> *reinterpret_cast<long double*>(val);
        return none;
      }
      case string8_v: {
        auto& str = *reinterpret_cast<std::string*>(val);
        size_t str_size;
        return error::eval([&] { return begin_sequence(str_size); },
                           [&] { str.resize(str_size);
                                 auto p = &str[0];
                                 auto data = reinterpret_cast<char_type*>(p);
                                 auto s = static_cast<streamsize>(str_size);
                                 return range_check(streambuf_.sgetn(data, s),
                                                    str_size); },
                           [&] { return end_sequence(); });
      }
      case string16_v: {
        auto& str = *reinterpret_cast<std::u16string*>(val);
        str.clear();
        size_t ns;
        return error::eval([&] { return begin_sequence(ns); },
                           [&] { return fill_range_c<uint16_t>(str, ns); },
                           [&] { return end_sequence(); });
      }
      case string32_v: {
        auto& str = *reinterpret_cast<std::u32string*>(val);
        str.clear();
        size_t ns;
        return error::eval([&] { return begin_sequence(ns); },
                           [&] { return fill_range_c<uint32_t>(str, ns); },
                           [&] { return end_sequence(); });
      }
    }
  }

  error range_check(std::streamsize got, size_t need) {
    if (got >= 0 && static_cast<size_t>(got) == need)
      return none;
    CAF_LOG_ERROR("range_check failed");
    return sec::end_of_stream;
  }

  template <class T>
  error apply_int(T& x) {
    T tmp;
    auto e = apply_raw(sizeof(T), &tmp);
    if (e)
      return e;
    x = detail::from_network_order(tmp);
    return none;
  }

  template <class T>
  error apply_float(T& x) {
    typename detail::ieee_754_trait<T>::packed_type tmp = 0;
    auto e = apply_int(tmp);
    if (e)
      return e;
    x = detail::unpack754(tmp);
    return none;
  }

private:
  Streambuf streambuf_;
};

} // namespace caf

