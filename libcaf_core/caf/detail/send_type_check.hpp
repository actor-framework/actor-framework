// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/implicit_conversions.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/response_type.hpp"

#include <type_traits>

namespace caf::detail {

template <class T>
struct signatures_of {
  using type = typename std::remove_pointer_t<T>::signatures;
};

template <class T>
using signatures_of_t = typename signatures_of<T>::type;

template <class SenderInterface, class Handle, class... Ts>
constexpr void send_type_check() {
  static_assert((detail::sendable<Ts> && ...),
                "at least one type has no ID, "
                "did you forgot to announce it via CAF_ADD_TYPE_ID?");
  using inputs = detail::type_list<detail::strip_and_convert_t<Ts>...>;
  using response_opt = response_type_unbox<signatures_of_t<Handle>, inputs>;
  static_assert(response_opt::valid, "receiver does not accept given message");
  if constexpr (!std::is_same_v<SenderInterface, none_t>) {
    using receiver_interface = signatures_of_t<Handle>;
    static_assert(!std::is_same_v<receiver_interface, none_t>,
                  "statically typed actors can only send() to other "
                  "statically typed actors; use anon_send() or request() when "
                  "communicating with dynamically typed actors");
    using response = typename response_opt::type;
    if constexpr (!std::is_same_v<response, type_list<void>>) {
      static_assert(response_type_unbox<SenderInterface, response>::valid,
                    "this actor does not accept the response message");
    }
  }
}

} // namespace caf::detail
