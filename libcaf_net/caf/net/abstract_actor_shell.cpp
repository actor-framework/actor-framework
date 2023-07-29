// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/abstract_actor_shell.hpp"

#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/action.hpp"
#include "caf/callback.hpp"
#include "caf/config.hpp"
#include "caf/detail/default_invoke_result_visitor.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/invoke_message_result.hpp"
#include "caf/logger.hpp"

namespace caf::net {

// -- constructors, destructors, and assignment operators ----------------------

abstract_actor_shell::abstract_actor_shell(actor_config& cfg,
                                           async::execution_context_ptr loop)
  : super(cfg), loop_(loop) {
  mailbox_.try_block();
  resume_ = make_action([this] {
    for (;;) {
      if (!consume_message() && try_block_mailbox())
        return;
    }
  });
}

abstract_actor_shell::~abstract_actor_shell() {
  // nop
}

// -- properties ---------------------------------------------------------------

bool abstract_actor_shell::terminated() const noexcept {
  return mailbox_.closed();
}

// -- state modifiers ----------------------------------------------------------

void abstract_actor_shell::quit(error reason) {
  cleanup(std::move(reason), nullptr);
}

// -- mailbox access -----------------------------------------------------------

mailbox_element_ptr abstract_actor_shell::next_message() {
  if (!mailbox_.blocked())
    return mailbox_.pop_front();
  return nullptr;
}

bool abstract_actor_shell::try_block_mailbox() {
  return mailbox_.try_block();
}

// -- message processing -------------------------------------------------------

bool abstract_actor_shell::consume_message() {
  CAF_LOG_TRACE("");
  if (auto msg = next_message()) {
    current_element_ = msg.get();
    CAF_LOG_RECEIVE_EVENT(current_element_);
    CAF_BEFORE_PROCESSING(this, *msg);
    auto mid = msg->mid;
    if (!mid.is_response()) {
      detail::default_invoke_result_visitor<abstract_actor_shell> visitor{this};
      if (auto result = bhvr_(msg->payload)) {
        visitor(*result);
      } else {
        auto fallback_result = (*fallback_)(this, msg->payload);
        visit(visitor, fallback_result);
      }
    } else if (auto i = multiplexed_responses_.find(mid);
               i != multiplexed_responses_.end()) {
      auto bhvr = std::move(i->second);
      multiplexed_responses_.erase(i);
      auto res = bhvr(msg->payload);
      if (!res) {
        CAF_LOG_DEBUG("got unexpected_response");
        auto err_msg = make_message(
          make_error(sec::unexpected_response, std::move(msg->payload)));
        bhvr(err_msg);
      }
    }
    CAF_AFTER_PROCESSING(this, invoke_message_result::consumed);
    CAF_LOG_SKIP_OR_FINALIZE_EVENT(invoke_message_result::consumed);
    return true;
  }
  return false;
}

void abstract_actor_shell::add_multiplexed_response_handler(
  message_id response_id, behavior bhvr) {
  if (bhvr.timeout() != infinite)
    request_response_timeout(bhvr.timeout(), response_id);
  multiplexed_responses_.emplace(response_id, std::move(bhvr));
}

// -- overridden functions of abstract_actor -----------------------------------

bool abstract_actor_shell::enqueue(mailbox_element_ptr ptr, execution_unit*) {
  CAF_ASSERT(ptr != nullptr);
  CAF_ASSERT(!getf(is_blocking_flag));
  CAF_LOG_TRACE(CAF_ARG(*ptr));
  CAF_LOG_SEND_EVENT(ptr);
  auto mid = ptr->mid;
  auto sender = ptr->sender;
  auto collects_metrics = getf(abstract_actor::collects_metrics_flag);
  if (collects_metrics) {
    ptr->set_enqueue_time();
    metrics_.mailbox_size->inc();
  }
  switch (mailbox().push_back(std::move(ptr))) {
    case intrusive::inbox_result::unblocked_reader: {
      CAF_LOG_ACCEPT_EVENT(true);
      std::unique_lock<std::mutex> guard{loop_mtx_};
      // The loop can only be null if this enqueue succeeds, then we close the
      // mailbox and reset loop_ in cleanup() before acquiring the mutex here.
      // Hence, the mailbox element has already been disposed and we can simply
      // skip any further processing.
      if (loop_)
        loop_->schedule(resume_);
      return true;
    }
    case intrusive::inbox_result::success:
      // Enqueued to a running actors' mailbox: nothing to do.
      CAF_LOG_ACCEPT_EVENT(false);
      return true;
    default: { // intrusive::inbox_result::queue_closed
      CAF_LOG_REJECT_EVENT();
      home_system().base_metrics().rejected_messages->inc();
      if (collects_metrics)
        metrics_.mailbox_size->dec();
      if (mid.is_request()) {
        detail::sync_request_bouncer f{exit_reason()};
        f(sender, mid);
      }
      return false;
    }
  }
}

mailbox_element* abstract_actor_shell::peek_at_next_mailbox_element() {
  return mailbox_.peek(make_message_id());
}

// -- overridden functions of local_actor --------------------------------------

void abstract_actor_shell::launch(execution_unit*, bool, bool hide) {
  CAF_PUSH_AID_FROM_PTR(this);
  CAF_LOG_TRACE(CAF_ARG(hide));
  CAF_ASSERT(!getf(is_blocking_flag));
  if (!hide)
    register_at_system();
}

bool abstract_actor_shell::cleanup(error&& fail_state, execution_unit* host) {
  CAF_LOG_TRACE(CAF_ARG(fail_state));
  // Clear mailbox.
  if (!mailbox_.closed()) {
    auto dropped = mailbox_.close(fail_state);
    while (dropped > 0 && getf(abstract_actor::collects_metrics_flag)) {
      auto val = static_cast<int64_t>(dropped);
      metrics_.mailbox_size->dec(val);
    }
  }
  // Detach from owner.
  {
    std::unique_lock<std::mutex> guard{loop_mtx_};
    loop_ = nullptr;
    resume_.dispose();
  }
  // Dispatch to parent's `cleanup` function.
  return super::cleanup(std::move(fail_state), host);
}

} // namespace caf::net
