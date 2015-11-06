/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_BINARY_DESERIALIZER_HPP
#define CAF_BINARY_DESERIALIZER_HPP

#include <string>
#include <cstddef>
#include <utility>

#include "caf/deserializer.hpp"

namespace caf {

/// Implements the deserializer interface with a binary serialization protocol.
class binary_deserializer : public deserializer {
public:
  binary_deserializer(execution_unit* ctx, const void* buf, size_t buf_size);

  binary_deserializer(execution_unit* ctx, const void* first, const void* last);

  /// Replaces the current read buffer.
  void set_rdbuf(const void* buf, size_t buf_size);

  /// Replaces the current read buffer.
  void set_rdbuf(const void* first, const void* last);

  /// Returns whether this deserializer has reached the end of its buffer.
  bool at_end() const;

  /// Compares the next `num_bytes` from the underlying buffer to `buf`
  /// with same semantics as `strncmp(this->pos_, buf, num_bytes) == 0`.
  bool buf_equals(const void* buf, size_t num_bytes);

  /// Moves the current read position in the buffer by `num_bytes`.
  binary_deserializer& advance(ptrdiff_t num_bytes);

  void begin_object(uint16_t& nr, std::string& name) override;
  void end_object() override;
  void begin_sequence(size_t& num_elements) override;
  void end_sequence() override;
  void apply_raw(size_t num_bytes, void* data) override;

protected:
  void apply_builtin(builtin in_out_type, void* in_out) override;

private:
  const void* pos_;
  const void* end_;
};

} // namespace caf

#endif // CAF_BINARY_DESERIALIZER_HPP
