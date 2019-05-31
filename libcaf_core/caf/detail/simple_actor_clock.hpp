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

#include <cstdint>
#include <map>
#include <memory>

#include "caf/actor_clock.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/detail/make_unique.hpp"
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

  using super = simple_actor_clock;

  struct event;

  struct delayed_event;

  /// Owning pointer to events.
  using unique_event_ptr = std::unique_ptr<event>;

  /// Owning pointer to delayed events.
  using unique_delayed_event_ptr = std::unique_ptr<delayed_event>;

  /// Maps timeouts to delayed events.
  using schedule_map = std::multimap<time_point, unique_delayed_event_ptr>;

  /// Maps actor IDs to schedule entries.
  using actor_lookup_map = std::multimap<actor_id, schedule_map::iterator>;

  /// Lists all possible subtypes of ::event.
  enum event_type {
    ordinary_timeout_type,
    multi_timeout_type,
    request_timeout_type,
    actor_msg_type,
    group_msg_type,
    ordinary_timeout_cancellation_type,
    multi_timeout_cancellation_type,
    request_timeout_cancellation_type,
    timeouts_cancellation_type,
    drop_all_type,
    shutdown_type,
  };

  /// Base class for clock events.
  struct event {
    event(event_type t) : subtype(t) {
      // nop
    }

    virtual ~event();

    /// Identifies the actual type of this object.
    event_type subtype;
  };

  /// An event with a timeout attached to it.
  struct delayed_event : event {
    delayed_event(event_type type, time_point due) : event(type), due(due) {
      // nop
    }

    /// Timestamp when this event should trigger.
    time_point due;

    /// Links back to the actor lookup map.
    actor_lookup_map::iterator backlink;
  };

  /// An ordinary timeout event for actors. Only one timeout for any timeout
  /// type can be active.
  struct ordinary_timeout final : delayed_event {
    static constexpr bool cancellable = true;

    ordinary_timeout(time_point due, strong_actor_ptr self, atom_value type,
                     uint64_t id)
      : delayed_event(ordinary_timeout_type, due),
        self(std::move(self)),
        type(type),
        id(id) {
      // nop
    }

    strong_actor_ptr self;
    atom_value type;
    uint64_t id;
  };

  /// An timeout event for actors that allows multiple active timers for the
  /// same type.
  struct multi_timeout final : delayed_event {
    static constexpr bool cancellable = true;

    multi_timeout(time_point due, strong_actor_ptr self, atom_value type,
                  uint64_t id)
      : delayed_event(multi_timeout_type, due),
        self(std::move(self)),
        type(type),
        id(id) {
      // nop
    }

    strong_actor_ptr self;
    atom_value type;
    uint64_t id;
  };

  /// A delayed `sec::request_timeout` error that gets cancelled when the
  /// request arrives in time.
  struct request_timeout final: delayed_event {
    static constexpr bool cancellable = true;

    request_timeout(time_point due, strong_actor_ptr self, message_id id)
      : delayed_event(request_timeout_type, due),
        self(std::move(self)),
        id(id) {
      // nop
    }

    strong_actor_ptr self;
    message_id id;
  };

  /// A delayed ::message to an actor.
  struct actor_msg final : delayed_event {
    static constexpr bool cancellable = false;

    actor_msg(time_point due, strong_actor_ptr receiver,
              mailbox_element_ptr content)
      : delayed_event(actor_msg_type, due),
        receiver(std::move(receiver)),
        content(std::move(content)) {
      // nop
    }

    strong_actor_ptr receiver;
    mailbox_element_ptr content;
  };

  /// A delayed ::message to a group.
  struct group_msg final : delayed_event {
    static constexpr bool cancellable = false;

    group_msg(time_point due, group target, strong_actor_ptr sender,
              message content)
      : delayed_event(group_msg_type, due),
        target(std::move(target)),
        sender(std::move(sender)),
        content(std::move(content)) {
      // nop
    }

    group target;
    strong_actor_ptr sender;
    message content;
  };

  /// Cancels a delayed event.
  struct cancellation : event {
    cancellation(event_type t, actor_id aid) : event(t), aid(aid) {
      //nop
    }

    actor_id aid;
  };

  /// Cancels matching ordinary timeouts.
  struct ordinary_timeout_cancellation final : cancellation {
    ordinary_timeout_cancellation(actor_id aid, atom_value type)
      : cancellation(ordinary_timeout_cancellation_type, aid), type(type) {
      // nop
    }

    atom_value type;
  };

  /// Cancels the matching multi timeout.
  struct multi_timeout_cancellation final : cancellation {
    multi_timeout_cancellation(actor_id aid, atom_value type, uint64_t id)
      : cancellation(ordinary_timeout_cancellation_type, aid),
        type(type),
        id(id) {
      // nop
    }

    atom_value type;
    uint64_t id;
  };

  /// Cancels a `sec::request_timeout` error.
  struct request_timeout_cancellation final : cancellation {
    request_timeout_cancellation(actor_id aid, message_id id)
      : cancellation(request_timeout_cancellation_type, aid), id(id) {
      // nop
    }

    message_id id;
  };

  /// Cancels all timeouts for an actor.
  struct timeouts_cancellation final : cancellation {
    timeouts_cancellation(actor_id aid)
      : cancellation(timeouts_cancellation_type, aid) {
      // nop
    }
  };

  /// Cancels all timeouts and messages.
  struct drop_all final : event {
    drop_all() : event(drop_all_type) {
      // nop
    }
  };

  /// Shuts down the actor clock.
  struct shutdown final : event {
    shutdown() : event(shutdown_type) {
      // nop
    }
  };

  // -- properties -------------------------------------------------------------

  const schedule_map& schedule() const {
    return schedule_;
  }

  const actor_lookup_map& actor_lookup() const {
    return actor_lookup_;
  }

  // -- convenience functions --------------------------------------------------

  /// Triggers all timeouts with timestamp <= now.
  /// @returns The number of triggered timeouts.
  /// @private
  size_t trigger_expired_timeouts();

  // -- overridden member functions --------------------------------------------

  void set_ordinary_timeout(time_point t, abstract_actor* self,
                            atom_value type, uint64_t id) override;

  void set_multi_timeout(time_point t, abstract_actor* self,
                         atom_value type, uint64_t id) override;

  void set_request_timeout(time_point t, abstract_actor* self,
                           message_id id) override;

  void cancel_ordinary_timeout(abstract_actor* self, atom_value type) override;

  void cancel_request_timeout(abstract_actor* self, message_id id) override;

  void cancel_timeouts(abstract_actor* self) override;

  void schedule_message(time_point t, strong_actor_ptr receiver,
                        mailbox_element_ptr content) override;

  void schedule_message(time_point t, group target, strong_actor_ptr sender,
                        message content) override;

  void cancel_all() override;

protected:
  // -- helper functions -------------------------------------------------------

  template <class Predicate>
  actor_lookup_map::iterator lookup(actor_id aid, Predicate pred) {
    auto e = actor_lookup_.end();
    auto range = actor_lookup_.equal_range(aid);
    if (range.first == range.second)
      return e;
    auto i = std::find_if(range.first, range.second, pred);
    return i != range.second ? i : e;
  }

  template <class Predicate>
  void cancel(actor_id aid, Predicate pred) {
    auto i = lookup(aid, pred);
    if (i != actor_lookup_.end()) {
      schedule_.erase(i->second);
      actor_lookup_.erase(i);
    }
  }

  template <class Predicate>
  void drop_lookup(actor_id aid, Predicate pred) {
    auto i = lookup(aid, pred);
    if (i != actor_lookup_.end())
      actor_lookup_.erase(i);
  }

  void handle(const ordinary_timeout_cancellation& x);

  void handle(const multi_timeout_cancellation& x);

  void handle(const request_timeout_cancellation& x);

  void handle(const timeouts_cancellation& x);

  void ship(delayed_event& x);

  template <class T>
  detail::enable_if_t<T::cancellable>
  add_schedule_entry(time_point t, std::unique_ptr<T> x) {
    auto id = x->self->id();
    auto i = schedule_.emplace(t, std::move(x));
    i->second->backlink = actor_lookup_.emplace(id, i);
  }

  template <class T>
  detail::enable_if_t<!T::cancellable>
  add_schedule_entry(time_point t, std::unique_ptr<T> x) {
    auto i = schedule_.emplace(t, std::move(x));
    i->second->backlink = actor_lookup_.end();
  }

  void add_schedule_entry(time_point t, std::unique_ptr<ordinary_timeout> x);

  template <class T>
  void add_schedule_entry(std::unique_ptr<T> x) {
    auto due = x->due;
    add_schedule_entry(due, std::move(x));
  }

  template <class T, class... Ts>
  void new_schedule_entry(time_point t, Ts&&... xs) {
    add_schedule_entry(t, detail::make_unique<T>(t, std::forward<Ts>(xs)...));
  }

  // -- member variables -------------------------------------------------------

  /// Timeout schedule.
  schedule_map schedule_;

  /// Secondary index for accessing timeouts by actor.
  actor_lookup_map actor_lookup_;
};

} // namespace detail
} // namespace caf
