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

#ifndef CAF_MAILBOX_ELEMENT_HPP
#define CAF_MAILBOX_ELEMENT_HPP

#include <cstddef>

#include "caf/extend.hpp"
#include "caf/message.hpp"
#include "caf/actor_addr.hpp"
#include "caf/message_id.hpp"
#include "caf/ref_counted.hpp"

#include "caf/detail/memory.hpp"
#include "caf/detail/embedded.hpp"
#include "caf/detail/disposer.hpp"
#include "caf/detail/tuple_vals.hpp"
#include "caf/detail/pair_storage.hpp"
#include "caf/detail/message_data.hpp"
#include "caf/detail/memory_cache_flag_type.hpp"

namespace caf {

class mailbox_element : public memory_managed {
public:
  static constexpr auto memory_cache_flag = detail::needs_embedding;

  mailbox_element* next; // intrusive next pointer
  mailbox_element* prev; // intrusive previous pointer
  bool marked;           // denotes if this node is currently processed
  actor_addr sender;
  message_id mid;
  message msg;           // 'content field'

  mailbox_element();
  mailbox_element(actor_addr sender, message_id id);
  mailbox_element(actor_addr sender, message_id id, message data);

  ~mailbox_element();

  mailbox_element(mailbox_element&&) = delete;
  mailbox_element(const mailbox_element&) = delete;
  mailbox_element& operator=(mailbox_element&&) = delete;
  mailbox_element& operator=(const mailbox_element&) = delete;

  using unique_ptr = std::unique_ptr<mailbox_element, detail::disposer>;

  static unique_ptr make(actor_addr sender, message_id id, message msg);

  template <class... Ts>
  static unique_ptr make_joint(actor_addr sender, message_id id, Ts&&... xs) {
    using value_storage =
      detail::tuple_vals<
        typename unbox_message_element<
          typename detail::strip_and_convert<Ts>::type
        >::type...
      >;
    std::integral_constant<size_t, 2> tk;
    using storage = detail::pair_storage<mailbox_element, value_storage>;
    auto ptr = detail::memory::create<storage>(tk, std::move(sender), id,
                                               std::forward<Ts>(xs)...);
    ptr->first.msg.reset(&(ptr->second), false);
    return unique_ptr{&(ptr->first)};
  }

  inline bool is_high_priority() const {
    return mid.is_high_priority();
  }
};

using mailbox_element_ptr = std::unique_ptr<mailbox_element, detail::disposer>;

} // namespace caf

#endif // CAF_MAILBOX_ELEMENT_HPP
