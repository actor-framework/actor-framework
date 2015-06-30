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

#include <limits>

#include "caf/binary_serializer.hpp"

namespace caf {

class binary_writer : public static_visitor<> {
public:
  using write_fun = binary_serializer::write_fun;

  explicit binary_writer(write_fun& sink) : out_(sink) {
    // nop
  }

  template <class T>
  static inline void write_int(write_fun& f, const T& value) {
    f(reinterpret_cast<const char*>(&value), sizeof(T));
  }

  static inline void write_string(write_fun& f, const std::string& str) {
    write_int(f, static_cast<uint32_t>(str.size()));
    f(str.data(), str.size());
  }

  void operator()(const bool& value) const {
    write_int(out_, static_cast<uint8_t>(value ? 1 : 0));
  }

  template <class T>
  typename std::enable_if<std::is_integral<T>::value>::type
  operator()(const T& value) const {
    write_int(out_, value);
  }

  template <class T>
  typename std::enable_if<std::is_floating_point<T>::value>::type
  operator()(const T& value) const {
    auto tmp = detail::pack754(value);
    write_int(out_, tmp);
  }

  // the IEEE-754 conversion does not work for long double
  // => fall back to string serialization (event though it sucks)
  void operator()(const long double& v) const {
    std::ostringstream oss;
    oss << std::setprecision(std::numeric_limits<long double>::digits) << v;
    write_string(out_, oss.str());
  }

  void operator()(const atom_value& val) const {
    (*this)(static_cast<uint64_t>(val));
  }

  void operator()(const std::string& str) const {
    write_string(out_, str);
  }

  void operator()(const std::u16string& str) const {
    // write size as 32 bit unsigned
    write_int(out_, static_cast<uint32_t>(str.size()));
    for (char16_t c : str) {
      // force writer to use exactly 16 bit
      write_int(out_, static_cast<uint16_t>(c));
    }
  }

  void operator()(const std::u32string& str) const {
    // write size as 32 bit unsigned
    write_int(out_, static_cast<uint32_t>(str.size()));
    for (char32_t c : str) {
      // force writer to use exactly 32 bit
      write_int(out_, static_cast<uint32_t>(c));
    }
  }

private:
  write_fun& out_;
};

void binary_serializer::begin_object(const uniform_type_info* uti) {
  auto nr = uti->type_nr();
  binary_writer::write_int(out_, nr);
  if (! nr) {
    binary_writer::write_string(out_, uti->name());
  }
}

void binary_serializer::end_object() {
  // nop
}

void binary_serializer::begin_sequence(size_t list_size) {
  binary_writer::write_int(out_, static_cast<uint32_t>(list_size));
}

void binary_serializer::end_sequence() {
  // nop
}

void binary_serializer::write_value(const primitive_variant& value) {
  binary_writer bw{out_};
  apply_visitor(bw, value);
}

void binary_serializer::write_raw(size_t num_bytes, const void* data) {
  out_(reinterpret_cast<const char*>(data), num_bytes);
}

} // namespace caf
