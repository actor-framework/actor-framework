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

#ifndef CAF_PURGE_REFS_HPP
#define CAF_PURGE_REFS_HPP

#include <functional>

#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

template <class T>
struct purge_refs_impl {
  using type = T;
};

template <class T>
struct purge_refs_impl<std::reference_wrapper<T>> {
  using type = T;
};

template <class T>
struct purge_refs_impl<std::reference_wrapper<const T>> {
  using type = T;
};

/// Removes references and reference wrappers.
template <class T>
struct purge_refs {
  using type =
    typename purge_refs_impl<
      typename std::decay<T>::type
    >::type;
};

} // namespace detail
} // namespace caf

#endif // CAF_PURGE_REFS_HPP
