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

#include "caf/serializer.hpp"

namespace caf {

/// Implements the serializer interface with a binary serialization protocol.
class binary_serializer final : public serializer {
public:
  // -- member types -----------------------------------------------------------

  using super = serializer;

  using buffer = std::vector<char>;

  // -- constructors, destructors, and assignment operators --------------------

  binary_serializer(actor_system& sys, buffer& buf);

  binary_serializer(execution_unit* ctx, buffer& buf);

  binary_serializer(binary_serializer&&) = default;

  binary_serializer& operator=(binary_serializer&&) = default;

  binary_serializer(const binary_serializer&) = delete;

  binary_serializer& operator=(const binary_serializer&) = delete;

  // -- position management ----------------------------------------------------

  /// Sets the write position to given offset.
  /// @pre `offset <= buf.size()`
  void seek(size_t offset);

  /// Jumps `num_bytes` forward. Resizes the buffer (filling it with zeros)
  /// when skipping past the end.
  void skip(size_t num_bytes);

  // -- overridden member functions --------------------------------------------

  error begin_object(uint16_t& typenr, std::string& name) override;

  error end_object() override;

  error begin_sequence(size_t& list_size) override;

  error end_sequence() override;

  error apply_raw(size_t num_bytes, void* data) override;

  // -- properties -------------------------------------------------------------

  buffer& buf() {
    return buf_;
  }

  const buffer& buf() const {
    return buf_;
  }

  size_t write_pos() const noexcept {
    return write_pos_;
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
  buffer& buf_;
  size_t write_pos_;
};

} // namespace caf
