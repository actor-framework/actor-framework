/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_DETAIL_PAIR_STORAGE_HPP
#define CAF_DETAIL_PAIR_STORAGE_HPP

#include "caf/extend.hpp"
#include "caf/ref_counted.hpp"

#include "caf/detail/embedded.hpp"
#include "caf/detail/memory_cache_flag_type.hpp"

namespace caf {
namespace detail {

// Reduces memory allocations by placing two independent types on one
// memory block. Typical use case is to combine the content of a message
// (tuple_vals) with its "context" (message ID and sender; mailbox_element).
//
// pair_storage<mailbox_element, tuple_vals<Ts...>>:
//
//     +-----------------------------------------------+
//     |                                               |
//     |     +------------+                            |
//     |     |            | intrusive_ptr              | intrusive_ptr
//     v     v            |                            |
// +------------+-------------------+---------------------+
// |  refcount  |  mailbox_element  |  tuple_vals<Ts...>  |
// +------------+-------------------+---------------------+
//                       ^                   ^
//                       |                   |
//            unique_ptr<mailbox_element,    |
//                       detail::disposer>   |
//                                           |
//                                           |
//                              intrusive_ptr<message_data>

template <class FirstType, class SecondType>
class pair_storage {
 public:
  union { embedded<FirstType> first; };
  union { embedded<SecondType> second; };

  template <class... Vs>
  pair_storage(intrusive_ptr<ref_counted> storage,
               std::integral_constant<size_t, 0>, Vs&&... vs)
      : first(storage),
        second(std::move(storage), std::forward<Vs>(vs)...) {
    // nop
  }

  template <class V0, class... Vs>
  pair_storage(intrusive_ptr<ref_counted> storage,
               std::integral_constant<size_t, 1>, V0&& v0, Vs&&... vs)
      : first(storage, std::forward<V0>(v0)),
        second(std::move(storage), std::forward<Vs>(vs)...) {
    // nop
  }

  template <class V0, class V1, class... Vs>
  pair_storage(intrusive_ptr<ref_counted> storage,
               std::integral_constant<size_t, 2>, V0&& v0, V1&& v1, Vs&&... vs)
      : first(storage, std::forward<V0>(v0), std::forward<V1>(v1)),
        second(std::move(storage), std::forward<Vs>(vs)...) {
    // nop
  }

  ~pair_storage() {
    // nop
  }

  static constexpr auto memory_cache_flag = provides_embedding;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_PAIR_STORAGE_HPP
