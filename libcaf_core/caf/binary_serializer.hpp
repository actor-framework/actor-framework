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
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/error_code.hpp"
#include "caf/fwd.hpp"
#include "caf/read_inspector.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"

namespace caf {

class binary_serializer : public read_inspector<binary_serializer> {
public:
  // -- member types -----------------------------------------------------------

  using result_type = error;

  // -- constructors, destructors, and assignment operators --------------------

  binary_serializer(actor_system& sys, byte_buffer& buf) noexcept;

  binary_serializer(execution_unit* ctx, byte_buffer& buf) noexcept
    : buf_(buf), write_pos_(buf.size()), context_(ctx) {
    // nop
  }

  binary_serializer(const binary_serializer&) = delete;

  binary_serializer& operator=(const binary_serializer&) = delete;

  // -- properties -------------------------------------------------------------

  /// Returns the current execution unit.
  execution_unit* context() const noexcept {
    return context_;
  }

  byte_buffer& buf() noexcept {
    return buf_;
  }

  const byte_buffer& buf() const noexcept {
    return buf_;
  }

  size_t write_pos() const noexcept {
    return write_pos_;
  }

  // -- position management ----------------------------------------------------

  /// Sets the write position to `offset`.
  /// @pre `offset <= buf.size()`
  void seek(size_t offset) noexcept {
    write_pos_ = offset;
  }

  /// Jumps `num_bytes` forward. Resizes the buffer (filling it with zeros)
  /// when skipping past the end.
  void skip(size_t num_bytes);

  // -- interface functions ----------------------------------------------------

  error_code<sec> begin_object(uint16_t nr, string_view name);

  error_code<sec> end_object();

  error_code<sec> begin_sequence(size_t list_size);

  error_code<sec> end_sequence();

  void apply(byte x);

  void apply(uint8_t x);

  void apply(uint16_t x);

  void apply(uint32_t x);

  void apply(uint64_t x);

  void apply(float x);

  void apply(double x);

  void apply(long double x);

  void apply(timespan x);

  void apply(timestamp x);

  void apply(string_view x);

  void apply(std::u16string_view x);

  void apply(std::u32string_view x);

  void apply(span<const byte> x);

  template <class T>
  std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>> apply(T x) {
    return apply(static_cast<std::make_unsigned_t<T>>(x));
  }

  template <class Enum>
  std::enable_if_t<std::is_enum<Enum>::value> apply(Enum x) {
    return apply(static_cast<std::underlying_type_t<Enum>>(x));
  }

  void apply(const std::vector<bool>& x);

private:
  /// Stores the serialized output.
  byte_buffer& buf_;

  /// Stores the current offset for writing.
  size_t write_pos_;

  /// Provides access to the ::proxy_registry and to the ::actor_system.
  execution_unit* context_;
};

} // namespace caf
