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
#include <vector>

#include "caf/byte.hpp"
#include "caf/deserializer.hpp"

namespace caf {

/// Implements the deserializer interface with a binary serialization protocol.
class binary_deserializer final : public deserializer {
public:
  // -- member types -----------------------------------------------------------

  using super = deserializer;

  // -- constructors, destructors, and assignment operators --------------------

  template <class ValueType>
  binary_deserializer(actor_system& sys, const ValueType* buf, size_t buf_size)
    : super(sys),
      current_(reinterpret_cast<const byte*>(buf)),
      end_(reinterpret_cast<const byte*>(buf) + buf_size) {
    static_assert(sizeof(ValueType) == 1, "sizeof(Value type) > 1");
  }

  template <class ValueType>
  binary_deserializer(execution_unit* ctx, const ValueType* buf,
                      size_t buf_size)
    : super(ctx),
      current_(reinterpret_cast<const byte*>(buf)),
      end_(reinterpret_cast<const byte*>(buf) + buf_size) {
    static_assert(sizeof(ValueType) == 1, "sizeof(Value type) > 1");
  }

  template <class BufferType>
  binary_deserializer(actor_system& sys, const BufferType& buf)
    : binary_deserializer(sys, buf.data(), buf.size()) {
    // nop
  }

  template <class BufferType>
  binary_deserializer(execution_unit* ctx, const BufferType& buf)
    : binary_deserializer(ctx, buf.data(), buf.size()) {
    // nop
  }

  // -- overridden member functions --------------------------------------------

  error begin_object(uint16_t& typenr, std::string& name) override;

  error end_object() override;

  error begin_sequence(size_t& list_size) override;

  error end_sequence() override;

  error apply_raw(size_t num_bytes, void* data) override;

  // -- properties -------------------------------------------------------------

  /// Returns the current read position.
  const byte* current() const {
    return current_;
  }

  /// Returns the past-the-end iterator.
  const byte* end() const {
    return end_;
  }

  /// Returns how many bytes are still available to read.
  size_t remaining() const;

  /// Jumps `num_bytes` forward.
  /// @pre `num_bytes <= remaining()`
  void skip(size_t num_bytes) {
    current_ += num_bytes;
  }

protected:
  error apply_impl(int8_t&) override;

  error apply_impl(uint8_t&) override;

  error apply_impl(int16_t&) override;

  error apply_impl(uint16_t&) override;

  error apply_impl(int32_t&) override;

  error apply_impl(uint32_t&) override;

  error apply_impl(int64_t&) override;

  error apply_impl(uint64_t&) override;

  error apply_impl(float&) override;

  error apply_impl(double&) override;

  error apply_impl(long double&) override;

  error apply_impl(std::string&) override;

  error apply_impl(std::u16string&) override;

  error apply_impl(std::u32string&) override;

private:
  bool range_check(size_t read_size) {
    return current_ + read_size <= end_;
  }

  const byte* current_;
  const byte* end_;
};

} // namespace caf
