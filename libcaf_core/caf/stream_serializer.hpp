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

#include <string>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <streambuf>
#include <type_traits>

#include "caf/sec.hpp"
#include "caf/config.hpp"
#include "caf/streambuf.hpp"
#include "caf/serializer.hpp"

#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"

namespace caf {

/// Implements the serializer interface with a binary serialization protocol.
template <class Streambuf>
class stream_serializer : public serializer {
  using streambuf_type = typename std::remove_reference<Streambuf>::type;
  using char_type = typename streambuf_type::char_type;
  using streambuf_base = std::basic_streambuf<char_type>;
  static_assert(std::is_base_of<streambuf_base, streambuf_type>::value,
                "Streambuf must inherit from std::streambuf");

public:
  template <class... Ts>
  explicit stream_serializer(actor_system& sys, Ts&&... xs)
    : serializer(sys),
      streambuf_{std::forward<Ts>(xs)...} {
  }

  template <class... Ts>
  explicit stream_serializer(execution_unit* ctx, Ts&&... xs)
    : serializer(ctx),
      streambuf_{std::forward<Ts>(xs)...} {
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
  explicit stream_serializer(S&& sb)
    : serializer(nullptr),
      streambuf_(std::forward<S>(sb)) {
  }

  error begin_object(uint16_t& typenr, std::string& name) override {
    return error::eval([&] { return apply(typenr); },
                       [&] { return typenr == 0 ? apply(name) : error{}; });

  }

  error end_object() override {
    return none;
  }

  error begin_sequence(size_t& list_size) override {
    // TODO: protect with `if constexpr (sizeof(size_t) > sizeof(uint32_t))`
    //       when switching to C++17
    CAF_ASSERT(list_size <= std::numeric_limits<uint32_t>::max());
    // Serialize a `size_t` always in 32-bit, to guarantee compatibility with
    // 32-bit nodes in the network.
    return varbyte_encode(static_cast<uint32_t>(list_size));
  }

  error end_sequence() override {
    return none;
  }

  error apply_raw(size_t num_bytes, void* data) override {
    auto ssize = static_cast<std::streamsize>(num_bytes);
    auto n = streambuf_.sputn(reinterpret_cast<char_type*>(data), ssize);
    if (n != ssize)
      return sec::end_of_stream;
    return none;
  }

protected:
  // Encode an unsigned integral type as variable-byte sequence.
  template <class T>
  error varbyte_encode(T x) {
    static_assert(std::is_unsigned<T>::value, "T must be an unsigned type");
    // For 64-bit values, the encoded representation cannot get larger than 10
    // bytes. A scratch space of 16 bytes suffices as upper bound.
    uint8_t buf[16];
    auto i = buf;
    while (x > 0x7f) {
      *i++ = (static_cast<uint8_t>(x) & 0x7f) | 0x80;
      x >>= 7;
    }
    *i++ = static_cast<uint8_t>(x) & 0x7f;
    auto res = streambuf_.sputn(reinterpret_cast<char_type*>(buf),
                                static_cast<std::streamsize>(i - buf));
    if (res != (i - buf))
      return sec::end_of_stream;
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
        return apply_int(detail::pack754(*reinterpret_cast<float*>(val)));
      case double_v:
        return apply_int(detail::pack754(*reinterpret_cast<double*>(val)));
      case ldouble_v: {
        // the IEEE-754 conversion does not work for long double
        // => fall back to string serialization (event though it sucks)
        std::ostringstream oss;
        oss << std::setprecision(std::numeric_limits<long double>::digits)
            << *reinterpret_cast<long double*>(val);
        auto tmp = oss.str();
        return apply(tmp);
      }
      case string8_v: {
        auto str = reinterpret_cast<std::string*>(val);
        auto s = str->size();
        auto data = reinterpret_cast<char_type*>(
                      const_cast<std::string::value_type*>(str->data()));
        return error::eval([&] { return begin_sequence(s); },
                           [&] { return apply_raw(str->size(),  data); },
                           [&] { return end_sequence(); });
      }
      case string16_v: {
        auto str = reinterpret_cast<std::u16string*>(val);
        auto s = str->size();
        // the standard does not guarantee that char16_t is exactly 16 bits...
        return error::eval([&] { return begin_sequence(s); },
                           [&] { return consume_range_c<uint16_t>(*str); },
                           [&] { return end_sequence(); });
      }
      case string32_v: {
        auto str = reinterpret_cast<std::u32string*>(val);
        auto s = str->size();
        // the standard does not guarantee that char32_t is exactly 32 bits...
        return error::eval([&] { return begin_sequence(s); },
                           [&] { return consume_range_c<uint32_t>(*str); },
                           [&] { return end_sequence(); });
      }
    }
  }

  template <class T>
  error apply_int(T x) {
    auto y = detail::to_network_order(x);
    return apply_raw(sizeof(T), &y);
  }

private:
  Streambuf streambuf_;
};

} // namespace caf

