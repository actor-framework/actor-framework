// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string>

#include "caf/actor.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/detail/serialized_size.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/intrusive/singly_linked.hpp"
#include "caf/intrusive/wdrr_fixed_multiplexed_queue.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/unit.hpp"
#include "caf/variant.hpp"

namespace caf::net {

class CAF_NET_EXPORT endpoint_manager_queue {
public:
  enum class element_type { event, message };

  class element : public intrusive::singly_linked<element> {
  public:
    explicit element(element_type tag) : tag_(tag) {
      // nop
    }

    virtual ~element();

    virtual size_t task_size() const noexcept = 0;

    element_type tag() const noexcept {
      return tag_;
    }

  private:
    element_type tag_;
  };

  using element_ptr = std::unique_ptr<element>;

  class event final : public element {
  public:
    struct resolve_request {
      uri locator;
      actor listener;
    };

    struct new_proxy {
      node_id peer;
      actor_id id;
    };

    struct local_actor_down {
      node_id observing_peer;
      actor_id id;
      error reason;
    };

    struct timeout {
      std::string type;
      uint64_t id;
    };

    event(uri locator, actor listener);

    event(node_id peer, actor_id proxy_id);

    event(node_id observing_peer, actor_id local_actor_id, error reason);

    event(std::string type, uint64_t id);

    ~event() override;

    size_t task_size() const noexcept override;

    /// Holds the event data.
    variant<resolve_request, new_proxy, local_actor_down, timeout> value;
  };

  using event_ptr = std::unique_ptr<event>;

  struct event_policy {
    using deficit_type = size_t;

    using task_size_type = size_t;

    using mapped_type = event;

    using unique_pointer = event_ptr;

    using queue_type = intrusive::drr_queue<event_policy>;

    constexpr event_policy(unit_t) {
      // nop
    }

    static constexpr task_size_type task_size(const event&) noexcept {
      return 1;
    }
  };

  class message : public element {
  public:
    /// Original message to a remote actor.
    mailbox_element_ptr msg;

    /// ID of the receiving actor.
    strong_actor_ptr receiver;

    message(mailbox_element_ptr msg, strong_actor_ptr receiver);

    ~message() override;

    size_t task_size() const noexcept override;
  };

  using message_ptr = std::unique_ptr<message>;

  struct message_policy {
    using deficit_type = size_t;

    using task_size_type = size_t;

    using mapped_type = message;

    using unique_pointer = message_ptr;

    using queue_type = intrusive::drr_queue<message_policy>;

    constexpr message_policy(unit_t) {
      // nop
    }

    static task_size_type task_size(const message& msg) noexcept {
      return detail::serialized_size(msg.msg->content());
    }
  };

  struct categorized {
    using deficit_type = size_t;

    using task_size_type = size_t;

    using mapped_type = element;

    using unique_pointer = element_ptr;

    constexpr categorized(unit_t) {
      // nop
    }

    template <class Queue>
    deficit_type quantum(const Queue&, deficit_type x) const noexcept {
      return x;
    }

    size_t id_of(const element& x) const noexcept {
      return static_cast<size_t>(x.tag());
    }
  };

  struct policy {
    using deficit_type = size_t;

    using task_size_type = size_t;

    using mapped_type = element;

    using unique_pointer = std::unique_ptr<element>;

    using queue_type = intrusive::wdrr_fixed_multiplexed_queue<
      categorized, event_policy::queue_type, message_policy::queue_type>;

    task_size_type task_size(const message& x) const noexcept {
      return x.task_size();
    }
  };

  using type = intrusive::fifo_inbox<policy>;
};

} // namespace caf::net
