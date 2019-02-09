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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "caf/deserializer.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/logger.hpp"
#include "caf/sec.hpp"

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

  error apply_impl(int8_t& x) override {
    return apply_raw(sizeof(int8_t), &x);
  }

  error apply_impl(uint8_t& x) override {
    return apply_raw(sizeof(uint8_t), &x);
  }

  error apply_impl(int16_t& x) override {
    return apply_int(x);
  }

  error apply_impl(uint16_t& x) override {
    return apply_int(x);
  }

  error apply_impl(int32_t& x) override {
    return apply_int(x);
  }

  error apply_impl(uint32_t& x) override {
    return apply_int(x);
  }

  error apply_impl(int64_t& x) override {
    return apply_int(x);
  }

  error apply_impl(uint64_t& x) override {
    return apply_int(x);
  }

  error apply_impl(float& x) override {
    return apply_float(x);
  }

  error apply_impl(double& x) override {
    return apply_float(x);
  }

  error apply_impl(long double& x) override {
    // The IEEE-754 conversion does not work for long double
    // => fall back to string serialization (even though it sucks).
    std::string tmp;
    if (auto err = apply(tmp))
      return err;
    std::istringstream iss{std::move(tmp)};
    iss >> x;
    return none;
  }

  error apply_impl(std::string& x) override {
    size_t str_size;
    if (auto err = begin_sequence(str_size))
      return err;
    x.resize(str_size);
    auto s = static_cast<streamsize>(str_size);
    auto data = reinterpret_cast<char_type*>(&x[0]);
    if (auto err = range_check(streambuf_.sgetn(data, s), str_size))
      return err;
    return end_sequence();
  }

  error apply_impl(std::u16string& x) override {
    size_t str_size;
    return error::eval([&] { return begin_sequence(str_size); },
                       [&] { return fill_range_c<uint16_t>(x, str_size); },
                       [&] { return end_sequence(); });
  }

  error apply_impl(std::u32string& x) override {
    size_t str_size;
    return error::eval([&] { return begin_sequence(str_size); },
                       [&] { return fill_range_c<uint32_t>(x, str_size); },
                       [&] { return end_sequence(); });
  }

  error range_check(std::streamsize got, size_t need) {
    if (got >= 0 && static_cast<size_t>(got) == need)
      return none;
    CAF_LOG_ERROR("range_check failed");
    return sec::end_of_stream;
  }

  template <class T>
  error apply_int(T& x) {
    typename std::make_unsigned<T>::type tmp = 0;
    if (auto err = apply_raw(sizeof(T), &tmp))
      return err;
    x = static_cast<T>(detail::from_network_order(tmp));
    return none;
  }

  template <class T>
  error apply_float(T& x) {
    typename detail::ieee_754_trait<T>::packed_type tmp = 0;
    if (auto err = apply_int(tmp))
      return err;
    x = detail::unpack754(tmp);
    return none;
  }

private:
  Streambuf streambuf_;
};

} // namespace caf

