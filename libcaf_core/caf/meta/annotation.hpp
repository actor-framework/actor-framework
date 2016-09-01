/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_META_ANNOTATION_HPP
#define CAF_META_ANNOTATION_HPP

#include <type_traits>

namespace caf {
namespace meta {

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

} // namespace meta
} // namespace caf

#endif // CAF_META_ANNOTATION_HPP
