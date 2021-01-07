// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <algorithm>
#include <cstddef>

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/detail/padded_size.hpp"
#include "caf/detail/stringification_inspector.hpp"
#include "caf/inspector_access.hpp"
#include "caf/serializer.hpp"

namespace caf::detail::default_function {

template <class T>
void destroy(void* ptr) noexcept {
  reinterpret_cast<T*>(ptr)->~T();
}

template <class T>
void default_construct(void* ptr) {
  new (ptr) T();
}

template <class T>
void copy_construct(void* ptr, const void* src) {
  new (ptr) T(*static_cast<const T*>(src));
}

template <class T>
bool save_binary(binary_serializer& sink, const void* ptr) {
  return sink.apply(*static_cast<const T*>(ptr));
}

template <class T>
bool load_binary(binary_deserializer& source, void* ptr) {
  return source.apply(*static_cast<T*>(ptr));
}

template <class T>
bool save(serializer& sink, const void* ptr) {
  return sink.apply(*static_cast<const T*>(ptr));
}

template <class T>
bool load(deserializer& source, void* ptr) {
  return source.apply(*static_cast<T*>(ptr));
}

template <class T>
void stringify(std::string& buf, const void* ptr) {
  stringification_inspector f{buf};
  auto unused = f.apply(*static_cast<const T*>(ptr));
  static_cast<void>(unused);
}

} // namespace caf::detail::default_function

namespace caf::detail {

template <class T>
meta_object make_meta_object(string_view type_name) {
  return {
    type_name,
    padded_size_v<T>,
    default_function::destroy<T>,
    default_function::default_construct<T>,
    default_function::copy_construct<T>,
    default_function::save_binary<T>,
    default_function::load_binary<T>,
    default_function::save<T>,
    default_function::load<T>,
    default_function::stringify<T>,
  };
}

} // namespace caf::detail
