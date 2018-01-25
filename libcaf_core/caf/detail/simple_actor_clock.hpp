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

#ifndef CAF_DETAIL_SIMPLE_ACTOR_CLOCK_HPP
#define CAF_DETAIL_SIMPLE_ACTOR_CLOCK_HPP

#include <map>

#include "caf/actor_clock.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/group.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message.hpp"
#include "caf/message_id.hpp"
#include "caf/variant.hpp"

namespace caf {
namespace detail {

class simple_actor_clock : public actor_clock {
public:
  // -- member types -----------------------------------------------------------

  /// Request for a `timeout_msg`.
  struct receive_timeout {
    strong_actor_ptr self;
    uint32_t id;
  };

  /// Request for a `sec::request_timeout` error.
  struct request_timeout {
    strong_actor_ptr self;
    message_id id;
  };

  /// Request for sending a message to an actor at a later time.
  struct actor_msg {
    strong_actor_ptr receiver;
    mailbox_element_ptr content;
  };

  /// Request for sending a message to a group at a later time.
  struct group_msg {
    group target;
    strong_actor_ptr sender;
    message content;
  };

  using value_type = variant<receive_timeout, request_timeout,
                             actor_msg, group_msg>;

  using map_type = std::multimap<time_point, value_type>;

  using secondary_map = std::multimap<abstract_actor*, map_type::iterator>;

  struct receive_predicate {
    bool operator()(const secondary_map::value_type& x) const noexcept;
  };

  struct request_predicate {
    message_id id;
    bool operator()(const secondary_map::value_type& x) const noexcept;
  };

  struct visitor {
    simple_actor_clock* thisptr;

    void operator()(receive_timeout& x);

    void operator()(request_timeout& x);

    void operator()(actor_msg& x);

    void operator()(group_msg& x);
  };

  void set_receive_timeout(time_point t, abstract_actor* self,
                           uint32_t id) override;

  void set_request_timeout(time_point t, abstract_actor* self,
                           message_id id) override;

  void cancel_receive_timeout(abstract_actor* self) override;

  void cancel_request_timeout(abstract_actor* self, message_id id) override;

  void cancel_timeouts(abstract_actor* self) override;

  void schedule_message(time_point t, strong_actor_ptr receiver,
                        mailbox_element_ptr content) override;

  void schedule_message(time_point t, group target, strong_actor_ptr sender,
                        message content) override;

  inline const map_type& schedule() const {
    return schedule_;
  }

  inline const secondary_map& actor_lookup() const {
    return actor_lookup_;
  }

protected:
  template <class Predicate>
  secondary_map::iterator lookup(abstract_actor* self, Predicate pred) {
    auto e = actor_lookup_.end();
    auto range = actor_lookup_.equal_range(self);
    if (range.first == range.second)
      return e;
    auto i = std::find_if(range.first, range.second, pred);
    return i != range.second ? i : e;
  }

  template <class Predicate>
  void cancel(abstract_actor* self, Predicate pred) {
    auto i = lookup(self, pred);
    if (i != actor_lookup_.end()) {
      schedule_.erase(i->second);
      actor_lookup_.erase(i);
    }
  }

  template <class Predicate>
  void drop_lookup(abstract_actor* self, Predicate pred) {
    auto i = lookup(self, pred);
    if (i != actor_lookup_.end())
      actor_lookup_.erase(i);
  }

  map_type schedule_;

  secondary_map actor_lookup_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_SIMPLE_ACTOR_CLOCK_HPP
