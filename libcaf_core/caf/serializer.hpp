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

#ifndef CAF_SERIALIZER_HPP
#define CAF_SERIALIZER_HPP

#include <string>
#include <cstddef> // size_t

#include "caf/uniform_type_info.hpp"
#include "caf/primitive_variant.hpp"

namespace caf {

class actor_namespace;
class uniform_type_info;

/// @ingroup TypeSystem
/// Technology-independent serialization interface.
class serializer {
public:
  serializer(const serializer&) = delete;
  serializer& operator=(const serializer&) = delete;

  /// @note `addressing` must be guaranteed to outlive the serializer
  serializer(actor_namespace* addressing = nullptr);

  virtual ~serializer();

  /// Begins serialization of an object of type `uti`.
  virtual void begin_object(const uniform_type_info* uti) = 0;

  /// Ends serialization of an object.
  virtual void end_object() = 0;

  /// Begins serialization of a sequence of size `num`.
  virtual void begin_sequence(size_t num) = 0;

  /// Ends serialization of a sequence.
  virtual void end_sequence() = 0;

  /// Writes a single value to the data sink.
  /// @param value A primitive data value.
  virtual void write_value(const primitive_variant& value) = 0;

  /// Writes a raw block of data.
  /// @param num_bytes The size of `data` in bytes.
  /// @param data Raw data.
  virtual void write_raw(size_t num_bytes, const void* data) = 0;

  inline actor_namespace* get_namespace() { return namespace_; }

  template <class T>
  inline serializer& write(const T& val) {
    write_value(val);
    return *this;
  }

  template <class T>
  inline serializer& write(const T& val, const uniform_type_info* uti) {
    uti->serialize(&val, this);
    return *this;
  }

private:
  actor_namespace* namespace_;
};

/// Serializes a value to `s`.
/// @param s A valid serializer.
/// @param what A value of an announced or primitive type.
/// @returns `s`
/// @relates serializer
template <class T>
serializer& operator<<(serializer& s, const T& what) {
  auto mtype = uniform_typeid<T>();
  if (mtype == nullptr) {
    throw std::logic_error("no uniform type info found for T");
  }
  mtype->serialize(&what, &s);
  return s;
}

} // namespace caf

#endif // CAF_SERIALIZER_HPP
