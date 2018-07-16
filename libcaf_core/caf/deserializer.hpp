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

#include <string>
#include <cstddef>
#include <utility>
#include <type_traits>

#include "caf/config.hpp"

#ifndef CAF_NO_EXCEPTIONS
#include <exception>
#endif // CAF_NO_EXCEPTIONS

#include "caf/fwd.hpp"
#include "caf/data_processor.hpp"

namespace caf {

/// @ingroup TypeSystem
/// Technology-independent deserialization interface.
class deserializer : public data_processor<deserializer> {
public:
  ~deserializer() override;

  using super = data_processor<deserializer>;

  static constexpr bool reads_state = false;
  static constexpr bool writes_state = true;

  // Boost Serialization compatibility
  using is_saving = std::false_type;
  using is_loading = std::true_type;

  explicit deserializer(actor_system& x);

  explicit deserializer(execution_unit* x = nullptr);
};

template <class T>
typename std::enable_if<
  std::is_same<
    error,
    decltype(std::declval<deserializer&>().apply(std::declval<T&>()))
  >::value
>::type
operator&(deserializer& source, T& x) {
  auto e = source.apply(x);
  if (e)
    CAF_RAISE_ERROR("error during serialization (using operator&)");
}

template <class T>
typename std::enable_if<
  std::is_same<
    error,
    decltype(std::declval<deserializer&>().apply(std::declval<T&>()))
  >::value,
  deserializer&
>::type
operator>>(deserializer& source, T& x) {
  source & x;
  return source;
}

} // namespace caf

