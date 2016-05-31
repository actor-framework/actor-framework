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
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/data_processor.hpp"

namespace caf {

/// @ingroup TypeSystem
/// Technology-independent serialization interface.
class serializer : public data_processor<serializer> {
public:
  using super = data_processor<serializer>;

  using is_saving = std::true_type;

  using is_loading = std::false_type;

  explicit serializer(actor_system& sys);

  explicit serializer(execution_unit* ctx = nullptr);

  virtual ~serializer();
};

template <class T>
typename std::enable_if<
  std::is_same<
    void,
    decltype(std::declval<serializer&>().apply(std::declval<T&>()))
  >::value,
  serializer&
>::type
operator<<(serializer& sink, const T& x) {
  // implementations are required to not change an object while serializing
  sink.apply(const_cast<T&>(x));
  return sink;
}

template <class T>
typename std::enable_if<
  std::is_same<
    void,
    decltype(std::declval<serializer&>().apply(std::declval<T&>()))
  >::value,
  serializer&
>::type
operator<<(serializer& sink, T& x) {
  sink.apply(x);
  return sink;
}

template <class T>
auto operator&(serializer& sink, T& x) -> decltype(sink.apply(x)) {
  sink.apply(x);
}

} // namespace caf

#endif // CAF_SERIALIZER_HPP
