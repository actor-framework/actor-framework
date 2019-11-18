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
#include <utility>

#include "caf/error_code.hpp"
#include "caf/fwd.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/write_inspector.hpp"

namespace caf {

/// Implements the deserializer interface with a binary serialization protocol.
class binary_deserializer : public write_inspector<binary_deserializer> {
public:
  // -- member types -----------------------------------------------------------

  using result_type = error_code<sec>;

  // -- constructors, destructors, and assignment operators --------------------

  template <class Container>
  binary_deserializer(actor_system& sys, const Container& input) noexcept
    : binary_deserializer(sys) {
    reset(as_bytes(make_span(input)));
  }

  template <class Container>
  binary_deserializer(execution_unit* ctx, const Container& input) noexcept
    : context_(ctx) {
    reset(as_bytes(make_span(input)));
  }

  binary_deserializer(execution_unit* ctx, const void* buf,
                      size_t size) noexcept
    : binary_deserializer(ctx, span{reinterpret_cast<const byte*>(buf), size}) {
    // nop
  }

  binary_deserializer(actor_system& sys, const void* buf, size_t size) noexcept
    : binary_deserializer(sys, span{reinterpret_cast<const byte*>(buf), size}) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  /// Returns how many bytes are still available to read.
  size_t remaining() const noexcept {
    return static_cast<size_t>(end_ - current_);
  }

  /// Returns the remaining bytes.
  span<const byte> remainder() const noexcept {
    return make_span(current_, end_);
  }

  /// Returns the current execution unit.
  execution_unit* context() const noexcept {
    return context_;
  }

  /// Jumps `num_bytes` forward.
  /// @pre `num_bytes <= remaining()`
  void skip(size_t num_bytes) noexcept;

  /// Assings a new input.
  void reset(span<const byte> bytes) noexcept;

  /// Returns the current read position.
  const byte* current() const noexcept {
    return current_;
  }

  /// Returns the end of the assigned memory block.
  const byte* end() const noexcept {
    return end_;
  }

  // -- overridden member functions --------------------------------------------

  result_type begin_object(uint16_t& typenr, std::string& name);

  result_type end_object() noexcept;

  result_type begin_sequence(size_t& list_size) noexcept;

  result_type end_sequence() noexcept;

  result_type apply(bool&) noexcept;

  result_type apply(byte&) noexcept;

  result_type apply(int8_t&) noexcept;

  result_type apply(uint8_t&) noexcept;

  result_type apply(int16_t&) noexcept;

  result_type apply(uint16_t&) noexcept;

  result_type apply(int32_t&) noexcept;

  result_type apply(uint32_t&) noexcept;

  result_type apply(int64_t&) noexcept;

  result_type apply(uint64_t&) noexcept;

  result_type apply(float&) noexcept;

  result_type apply(double&) noexcept;

  result_type apply(long double&);

  result_type apply(span<byte>) noexcept;

  result_type apply(std::string&);

  result_type apply(std::u16string&);

  result_type apply(std::u32string&);

  template <class Enum, class = std::enable_if_t<std::is_enum<Enum>::value>>
  auto apply(Enum& x) noexcept {
    return apply(reinterpret_cast<std::underlying_type_t<Enum>&>(x));
  }

  result_type apply(std::vector<bool>& xs);

private:
  explicit binary_deserializer(actor_system& sys) noexcept;

  /// Checks whether we can read `read_size` more bytes.
  bool range_check(size_t read_size) const noexcept {
    return current_ + read_size <= end_;
  }

  /// Points to the current read position.
  const byte* current_;

  /// Points to the end of the assigned memory block.
  const byte* end_;

  /// Provides access to the ::proxy_registry and to the ::actor_system.
  execution_unit* context_;
};

} // namespace caf
