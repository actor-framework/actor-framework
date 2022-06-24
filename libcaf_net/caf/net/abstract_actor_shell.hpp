// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_traits.hpp"
#include "caf/async/execution_context.hpp"
#include "caf/callback.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/local_actor.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/net/fwd.hpp"
#include "caf/none.hpp"
#include "caf/policy/normal_messages.hpp"
#include "caf/unordered_flat_map.hpp"

namespace caf::net {

class CAF_NET_EXPORT abstract_actor_shell : public local_actor,
                                            public non_blocking_actor_base {
public:
  // -- member types -----------------------------------------------------------

  using super = local_actor;

  struct mailbox_policy {
    using queue_type = intrusive::drr_queue<policy::normal_messages>;

    using deficit_type = policy::normal_messages::deficit_type;

    using mapped_type = policy::normal_messages::mapped_type;

    using unique_pointer = policy::normal_messages::unique_pointer;
  };

  using mailbox_type = intrusive::fifo_inbox<mailbox_policy>;

  using fallback_handler_sig = result<message>(abstract_actor_shell*, message&);

  using fallback_handler = unique_callback_ptr<fallback_handler_sig>;

  // -- constructors, destructors, and assignment operators --------------------

  abstract_actor_shell(actor_config& cfg, async::execution_context_ptr loop);

  ~abstract_actor_shell() override;

  // -- properties -------------------------------------------------------------

  bool terminated() const noexcept;

  // -- state modifiers --------------------------------------------------------

  /// Detaches the shell from its loop and closes the mailbox.
  void quit(error reason);

  /// Overrides the default handler for unexpected messages.
  template <class F>
  void set_fallback(F f) {
    using msg_res_t = result<message>;
    using self_ptr_t = abstract_actor_shell*;
    if constexpr (std::is_invocable_r_v<msg_res_t, F, self_ptr_t, message&>) {
      fallback_ = make_type_erased_callback(std::move(f));
    } else {
      static_assert(std::is_invocable_r_v<msg_res_t, F, message&>,
                    "illegal signature for the fallback handler: must be "
                    "'result<message>(message&)' or "
                    "'result<message>(abstract_actor_shell*, message&)'");
      auto g = [f = std::move(f)](self_ptr_t, message& msg) mutable {
        return f(msg);
      };
      fallback_ = make_type_erased_callback(std::move(g));
    }
  }

  // -- mailbox access ---------------------------------------------------------

  auto& mailbox() noexcept {
    return mailbox_;
  }

  /// Dequeues and returns the next message from the mailbox or returns
  /// `nullptr` if the mailbox is empty.
  mailbox_element_ptr next_message();

  /// Tries to put the mailbox into the `blocked` state, causing the next
  /// enqueue to register the owning socket manager for write events.
  bool try_block_mailbox();

  // -- message processing -----------------------------------------------------

  /// Adds a callback for a multiplexed response.
  void add_multiplexed_response_handler(message_id response_id, behavior bhvr);

  // -- overridden functions of abstract_actor ---------------------------------

  using abstract_actor::enqueue;

  bool enqueue(mailbox_element_ptr ptr, execution_unit* eu) override;

  mailbox_element* peek_at_next_mailbox_element() override;

  // -- overridden functions of local_actor ------------------------------------

  void launch(execution_unit* eu, bool lazy, bool hide) override;

  bool cleanup(error&& fail_state, execution_unit* host) override;

protected:
  void set_behavior_impl(behavior bhvr) {
    bhvr_ = std::move(bhvr);
  }

private:
  /// Stores incoming actor messages.
  mailbox_type mailbox_;

  /// Guards access to loop_.
  std::mutex loop_mtx_;

  /// Points to the loop in which this "actor" runs (nullptr after calling
  /// quit).
  async::execution_context_ptr loop_;

  /// Handler for consuming messages from the mailbox.
  behavior bhvr_;

  /// Handler for unexpected messages.
  fallback_handler fallback_;

  /// Stores callbacks for multiplexed responses.
  unordered_flat_map<message_id, behavior> multiplexed_responses_;

  /// Callback for processing the next message on the event loop.
  action resume_;

  /// Dequeues and processes the next message from the mailbox.
  bool consume_message();
};

} // namespace caf::net
