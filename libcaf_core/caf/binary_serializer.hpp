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
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/error_code.hpp"
#include "caf/fwd.hpp"
#include "caf/save_inspector.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"

namespace caf {

/// Serializes objects into a sequence of bytes.
class CAF_CORE_EXPORT binary_serializer : public save_inspector {
public:
  // -- member types -----------------------------------------------------------

  using container_type = byte_buffer;

  using value_type = byte;

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

  static constexpr bool has_human_readable_format() noexcept {
    return false;
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

  constexpr bool begin_object(string_view) {
    return ok;
  }

  constexpr bool end_object() {
    return ok;
  }

  constexpr bool begin_field(string_view) noexcept {
    return ok;
  }

  bool begin_field(string_view, bool is_present);

  bool begin_field(string_view, span<const type_id_t> types, size_t index);

  bool begin_field(string_view, bool is_present, span<const type_id_t> types,
                   size_t index);

  constexpr bool end_field() {
    return ok;
  }

  constexpr bool begin_tuple(size_t) {
    return ok;
  }

  constexpr bool end_tuple() {
    return ok;
  }

  bool begin_sequence(size_t list_size);

  constexpr bool end_sequence() {
    return ok;
  }

  bool value(byte x);

  bool value(int8_t x);

  bool value(uint8_t x);

  bool value(int16_t x);

  bool value(uint16_t x);

  bool value(int32_t x);

  bool value(uint32_t x);

  bool value(int64_t x);

  bool value(uint64_t x);

  bool value(float x);

  bool value(double x);

  bool value(long double x);

  bool value(string_view x);

  bool value(const std::u16string& x);

  bool value(const std::u32string& x);

  bool value(span<const byte> x);

  bool value(const std::vector<bool>& x);

  // -- DSL entry point --------------------------------------------------------

  template <class T>
  constexpr auto object(T&) noexcept {
    return object_t<binary_serializer>{type_name_or_anonymous<T>(), this};
  }

private:
  /// Stores the serialized output.
  byte_buffer& buf_;

  /// Stores the current offset for writing.
  size_t write_pos_;

  /// Provides access to the ::proxy_registry and to the ::actor_system.
  execution_unit* context_;
};

} // namespace caf
