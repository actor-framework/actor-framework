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

#ifndef CAF_DESERIALIZER_HPP
#define CAF_DESERIALIZER_HPP

#include <string>
#include <cstddef>

#include "caf/primitive_variant.hpp"
#include "caf/uniform_type_info.hpp"

namespace caf {

class actor_namespace;
class uniform_type_info;

/// @ingroup TypeSystem
/// Technology-independent deserialization interface.
class deserializer {
public:
  explicit deserializer(actor_namespace* ns = nullptr);

  deserializer(const deserializer&) = delete;
  deserializer& operator=(const deserializer&) = delete;

  virtual ~deserializer();

  /// Begins deserialization of a new object.
  virtual const uniform_type_info* begin_object() = 0;

  /// Ends deserialization of an object.
  virtual void end_object() = 0;

  /// Begins deserialization of a sequence.
  /// @returns The size of the sequence.
  virtual size_t begin_sequence() = 0;

  /// Ends deserialization of a sequence.
  virtual void end_sequence() = 0;

  /// Reads a primitive value from the data source.
  virtual void read_value(primitive_variant& storage) = 0;

  /// Reads a value of type `T` from the data source.
  /// @note `T` must be a primitive type.
  template <class T>
  inline T read() {
    primitive_variant val{T()};
    read_value(val);
    // works around the all-to-strict boost::strict_get of boost::variant
    using result =
      typename std::conditional<
        std::is_same<T, char>::value,
        int8_t,
        T
      >::type;
    return std::move(get<result>(val));
  }

  template <class T>
  inline T read(const uniform_type_info* uti) {
    T result;
    uti->deserialize(&result, this);
    return result;
  }

  template <class T>
  inline deserializer& read(T& storage) {
    primitive_variant val{T()};
    read_value(val);
    storage = std::move(get<T>(val));
    return *this;
  }

  template <class T>
  inline deserializer& read(T& storage, const uniform_type_info* uti) {
    uti->deserialize(&storage, this);
    return *this;
  }

  /// Reads a raw memory block.
  virtual void read_raw(size_t num_bytes, void* storage) = 0;

  inline actor_namespace* get_namespace() {
    return namespace_;
  }

  template <class Buffer>
  void read_raw(size_t num_bytes, Buffer& storage) {
    storage.resize(num_bytes);
    read_raw(num_bytes, storage.data());
  }

protected:
  actor_namespace* namespace_;
};

/// Serializes a value to `s`.
/// @relates serializer
template <class T>
typename std::enable_if<
  detail::is_primitive<T>::value,
  deserializer&
>::type
operator>>(deserializer& source, T& value) {
  return source.read(value);
}

/// Serializes a value to `s`.
/// @relates serializer
template <class T>
typename std::enable_if<
  ! detail::is_primitive<T>::value,
  deserializer&
>::type
operator>>(deserializer& source, T& value) {
  return source.read(value, uniform_typeid<T>());
}

template <class T>
void operator&(deserializer& source, T& value) {
  source >> value;
}

} // namespace caf

#endif // CAF_DESERIALIZER_HPP
