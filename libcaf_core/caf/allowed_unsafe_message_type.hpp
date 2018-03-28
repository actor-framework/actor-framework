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

// This file is referenced in the manual, do not modify without updating refs!
// ConfiguringActorApplications: 50-54

#pragma once

#include <type_traits>

namespace caf {

/// Template specializations can whitelist individual
/// types for unsafe message passing operations.
template <class T>
struct allowed_unsafe_message_type : std::false_type {};

template <class T>
struct is_allowed_unsafe_message_type : allowed_unsafe_message_type<T> {};

template <class T>
struct is_allowed_unsafe_message_type<T&> : allowed_unsafe_message_type<T> {};

template <class T>
struct is_allowed_unsafe_message_type<T&&> : allowed_unsafe_message_type<T> {};

template <class T>
struct is_allowed_unsafe_message_type<const T&>
    : allowed_unsafe_message_type<T> {};

} // namespace caf

#define CAF_ALLOW_UNSAFE_MESSAGE_TYPE(type_name)                               \
  namespace caf {                                                              \
  template <>                                                                  \
  struct allowed_unsafe_message_type<type_name> : std::true_type {};           \
  }

