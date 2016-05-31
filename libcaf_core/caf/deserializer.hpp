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
#include <utility>
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/data_processor.hpp"

namespace caf {

/// @ingroup TypeSystem
/// Technology-independent deserialization interface.
class deserializer : public data_processor<deserializer> {
public:
  ~deserializer();

  using super = data_processor<deserializer>;

  using is_saving = std::false_type;

  using is_loading = std::true_type;

  explicit deserializer(actor_system& sys);

  explicit deserializer(execution_unit* ctx = nullptr);
};

/// Reads `x` from `source`.
/// @relates serializer
template <class T>
typename std::enable_if<
  std::is_same<
    void,
    decltype(std::declval<deserializer&>().apply(std::declval<T&>()))
  >::value,
  deserializer&
>::type
operator>>(deserializer& source, T& x) {
  source.apply(x);
  return source;
}

template <class T>
auto operator&(deserializer& source, T& x) -> decltype(source.apply(x)) {
  source.apply(x);
}

} // namespace caf

#endif // CAF_DESERIALIZER_HPP
