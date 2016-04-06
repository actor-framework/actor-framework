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

#include "caf/actor.hpp"
#include "caf/extend.hpp"
#include "caf/message.hpp"
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

  using forwarding_stack = std::vector<strong_actor_ptr>;

  // intrusive pointer to the next mailbox element
  mailbox_element* next;
  // intrusive pointer to the previous mailbox element
  mailbox_element* prev;
  // avoid multi-processing in blocking actors via flagging
  bool marked;
  // source of this message and receiver of the final response
  strong_actor_ptr sender;
  // denotes whether this a sync or async message
  message_id mid;
  // stages.back() is the next actor in the forwarding chain,
  // if this is empty then the original sender receives the response
  forwarding_stack stages;
  // content of this element
  message msg;

  mailbox_element();

  mailbox_element(strong_actor_ptr sender, message_id id,
                  forwarding_stack stages);

  mailbox_element(strong_actor_ptr sender, message_id id,
                  forwarding_stack stages, message data);

  ~mailbox_element();

  mailbox_element(mailbox_element&&) = delete;
  mailbox_element(const mailbox_element&) = delete;
  mailbox_element& operator=(mailbox_element&&) = delete;
  mailbox_element& operator=(const mailbox_element&) = delete;

  using unique_ptr = std::unique_ptr<mailbox_element, detail::disposer>;

  static unique_ptr make(strong_actor_ptr sender, message_id id,
                         forwarding_stack stages, message msg);

  template <class... Ts>
  static unique_ptr make_joint(strong_actor_ptr sender, message_id id,
                               forwarding_stack stages, Ts&&... xs) {
    using value_storage =
      detail::tuple_vals<
        typename unbox_message_element<
          typename detail::strip_and_convert<Ts>::type
        >::type...
      >;
    std::integral_constant<size_t, 3> tk;
    using storage = detail::pair_storage<mailbox_element, value_storage>;
    auto ptr = detail::memory::create<storage>(tk, std::move(sender), id,
                                               std::move(stages),
                                               std::forward<Ts>(xs)...);
    ptr->first.msg.reset(&(ptr->second), false);
    return unique_ptr{&(ptr->first)};
  }

  inline bool is_high_priority() const {
    return mid.is_high_priority();
  }
};

using mailbox_element_ptr = std::unique_ptr<mailbox_element, detail::disposer>;

std::string to_string(const mailbox_element&);

} // namespace caf

#endif // CAF_MAILBOX_ELEMENT_HPP
