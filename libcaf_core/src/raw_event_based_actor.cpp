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

#include "caf/raw_event_based_actor.hpp"

#include "caf/detail/default_invoke_result_visitor.hpp"

namespace caf {

raw_event_based_actor::raw_event_based_actor(actor_config& cfg)
  : event_based_actor(cfg) {
  // nop
}

invoke_message_result raw_event_based_actor::consume(mailbox_element& x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  current_element_ = &x;
  CAF_LOG_RECEIVE_EVENT(current_element_);
  CAF_BEFORE_PROCESSING(this, x);
  // Wrap the actual body for the function.
  auto body = [this, &x] {
    // short-circuit awaited responses
    if (!awaited_responses_.empty()) {
      auto& pr = awaited_responses_.front();
      // skip all messages until we receive the currently awaited response
      if (x.mid != pr.first)
        return invoke_message_result::skipped;
      if (!pr.second(x.content())) {
        // try again with error if first attempt failed
        auto msg = make_message(
          make_error(sec::unexpected_response, x.move_content_to_message()));
        pr.second(msg);
      }
      awaited_responses_.pop_front();
      return invoke_message_result::consumed;
    }
    // handle multiplexed responses
    if (x.mid.is_response()) {
      auto mrh = multiplexed_responses_.find(x.mid);
      // neither awaited nor multiplexed, probably an expired timeout
      if (mrh == multiplexed_responses_.end())
        return invoke_message_result::dropped;
      if (!mrh->second(x.content())) {
        // try again with error if first attempt failed
        auto msg = make_message(
          make_error(sec::unexpected_response, x.move_content_to_message()));
        mrh->second(msg);
      }
      multiplexed_responses_.erase(mrh);
      return invoke_message_result::consumed;
    }
    auto& content = x.content();
    //  handle timeout messages
    if (content.match_elements<timeout_msg>()) {
      auto& tm = content.get_as<timeout_msg>(0);
      auto tid = tm.timeout_id;
      CAF_ASSERT(x.mid.is_async());
      if (is_active_receive_timeout(tid)) {
        CAF_LOG_DEBUG("handle timeout message");
        if (bhvr_stack_.empty())
          return invoke_message_result::dropped;
        bhvr_stack_.back().handle_timeout();
        return invoke_message_result::consumed;
      }
      CAF_LOG_DEBUG("dropped expired timeout message");
      return invoke_message_result::dropped;
    }
    // handle everything else as ordinary message
    detail::default_invoke_result_visitor<event_based_actor> visitor{this};
    bool skipped = false;
    auto had_timeout = getf(has_timeout_flag);
    if (had_timeout)
      unsetf(has_timeout_flag);
    // restore timeout at scope exit if message was skipped
    auto timeout_guard = detail::make_scope_guard([&] {
      if (skipped && had_timeout)
        setf(has_timeout_flag);
    });
    auto call_default_handler = [&] {
      auto sres = call_handler(default_handler_, this, x);
      switch (sres.flag) {
        default:
          break;
        case rt_error:
        case rt_value:
          visitor.visit(sres);
          break;
        case rt_skip:
          skipped = true;
      }
    };
    if (bhvr_stack_.empty()) {
      call_default_handler();
      return !skipped ? invoke_message_result::consumed
                      : invoke_message_result::skipped;
    }
    auto& bhvr = bhvr_stack_.back();
    switch (bhvr(visitor, x.content())) {
      default:
        break;
      case match_case::skip:
        skipped = true;
        break;
      case match_case::no_match:
        call_default_handler();
    }
    return !skipped ? invoke_message_result::consumed
                    : invoke_message_result::skipped;
  };
  // Post-process the returned value from the function body.
  auto result = body();
  CAF_AFTER_PROCESSING(this, result);
  CAF_LOG_SKIP_OR_FINALIZE_EVENT(result);
  return result;
}

} // namespace caf
