// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

namespace caf::meta {

/// Type tag for all meta annotations in CAF.
struct annotation {
  constexpr annotation() {
    // nop
  }
};

template <class T>
struct is_annotation {
  static constexpr bool value = std::is_base_of<annotation, T>::value;
};

template <class T>
struct is_annotation<T&> : is_annotation<T> {};

template <class T>
struct is_annotation<const T&> : is_annotation<T> {};

template <class T>
struct is_annotation<T&&> : is_annotation<T> {};

template <class T>
constexpr bool is_annotation_v = is_annotation<T>::value;

} // namespace caf::meta
