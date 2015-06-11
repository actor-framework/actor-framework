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

#include "caf/deserializer.hpp"

namespace caf {

/// Implements the deserializer interface with a binary serialization protocol.
class binary_deserializer : public deserializer {

  using super = deserializer;

public:

  binary_deserializer(const void* buf, size_t buf_size,
            actor_namespace* ns = nullptr);

  binary_deserializer(const void* begin, const void* end_,
            actor_namespace* ns = nullptr);

  const uniform_type_info* begin_object() override;
  void end_object() override;
  size_t begin_sequence() override;
  void end_sequence() override;
  void read_value(primitive_variant& storage) override;
  void read_raw(size_t num_bytes, void* storage) override;

  /// Replaces the current read buffer.
  void set_rdbuf(const void* buf, size_t buf_size);

  /// Replaces the current read buffer.
  void set_rdbuf(const void* begin, const void* end_);

private:

  const void* pos_;
  const void* end_;

};

template <class T>
inline binary_deserializer& operator>>(binary_deserializer& lhs, T& rhs) {
  rhs = lhs.read<T>();
  return lhs;
}

} // namespace caf

#endif // CAF_BINARY_DESERIALIZER_HPP
