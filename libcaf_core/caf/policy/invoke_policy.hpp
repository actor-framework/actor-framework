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

#ifndef CAF_POLICY_INVOKE_POLICY_HPP
#define CAF_POLICY_INVOKE_POLICY_HPP

#include <memory>
#include <type_traits>

#include "caf/none.hpp"

#include "caf/on.hpp"
#include "caf/behavior.hpp"
#include "caf/to_string.hpp"
#include "caf/message_id.hpp"
#include "caf/exit_reason.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/system_messages.hpp"
#include "caf/response_promise.hpp"

#include "caf/detail/memory.hpp"
#include "caf/detail/logging.hpp"
#include "caf/detail/scope_guard.hpp"

namespace caf {
namespace policy {

enum invoke_message_result {
  im_success,
  im_skipped,
  im_dropped
};

/**
 * Base class for invoke policies.
 */
class invoke_policy {
 public:
  enum class msg_type {
    normal_exit,           // an exit message with normal exit reason
    non_normal_exit,       // an exit message with abnormal exit reason
    expired_timeout,       // an 'old & obsolete' timeout
    inactive_timeout,      // a currently inactive timeout
    expired_sync_response, // a sync response that already timed out
    timeout,               // triggers currently active timeout
    timeout_response,      // triggers timeout of a sync message
    ordinary,              // an asynchronous message or sync. request
    sync_response          // a synchronous response
  };

  // the workflow of invoke_message (im) is as follows:
  // - should_skip? if yes: return im_skipped
  // - msg is ordinary message? if yes:
  //   - begin(...) -> prepares a self for message handling
  //   - self could process message?
  //   - yes: cleanup()
  //   - no: revert(...) -> set self back to state it had before begin()
  template <class Actor>
  invoke_message_result invoke_message(Actor* self, mailbox_element_ptr& node,
                                       behavior& fun,
                                       message_id awaited_response) {
    CAF_LOG_TRACE("");
    bool handle_sync_failure_on_mismatch = true;
    switch (this->filter_msg(self, *node)) {
      case msg_type::normal_exit:
        CAF_LOG_DEBUG("dropped normal exit signal");
        return im_dropped;
      case msg_type::expired_sync_response:
        CAF_LOG_DEBUG("dropped expired sync response");
        return im_dropped;
      case msg_type::expired_timeout:
        CAF_LOG_DEBUG("dropped expired timeout message");
        return im_dropped;
      case msg_type::inactive_timeout:
        CAF_LOG_DEBUG("skipped inactive timeout message");
        return im_skipped;
      case msg_type::non_normal_exit:
        CAF_LOG_DEBUG("handled non-normal exit signal");
        // this message was handled
        // by calling self->quit(...)
        return im_success;
      case msg_type::timeout: {
        CAF_LOG_DEBUG("handle timeout message");
        auto& tm = node->msg.get_as<timeout_msg>(0);
        self->handle_timeout(fun, tm.timeout_id);
        if (awaited_response.valid()) {
          self->mark_arrived(awaited_response);
          self->remove_handler(awaited_response);
        }
        return im_success;
      }
      case msg_type::timeout_response:
        handle_sync_failure_on_mismatch = false;
        CAF_ANNOTATE_FALLTHROUGH;
      case msg_type::sync_response:
        CAF_LOG_DEBUG("handle as synchronous response: "
                 << CAF_TARG(node->msg, to_string) << ", "
                 << CAF_MARG(node->mid, integer_value) << ", "
                 << CAF_MARG(awaited_response, integer_value));
        if (awaited_response.valid() && node->mid == awaited_response) {
          node.swap(self->current_element());
          auto res = invoke_fun(self, fun);
          if (!res && handle_sync_failure_on_mismatch) {
            CAF_LOG_WARNING("sync failure occured in actor "
                     << "with ID " << self->id());
            self->handle_sync_failure();
          }
          self->mark_arrived(awaited_response);
          self->remove_handler(awaited_response);
          node.swap(self->current_element());
          return im_success;
        }
        return im_skipped;
      case msg_type::ordinary:
        if (!awaited_response.valid()) {
          node.swap(self->current_element());
          auto res = invoke_fun(self, fun);
          node.swap(self->current_element());
          if (res) {
            return im_success;
          }
        }
        CAF_LOG_DEBUG_IF(awaited_response.valid(),
                  "ignored message; await response: "
                    << awaited_response.integer_value());
        return im_skipped;
    }
    // should be unreachable
    CAF_CRITICAL("invalid message type");
  }

  template <class Actor>
  response_promise fetch_response_promise(Actor* cl, int) {
    return cl->make_response_promise();
  }

  template <class Actor>
  response_promise fetch_response_promise(Actor*, response_promise& hdl) {
    return std::move(hdl);
  }

  // - extracts response message from handler
  // - returns true if fun was successfully invoked
  template <class Actor, class Fun, class MaybeResponseHdl = int>
  optional<message> invoke_fun(Actor* self, Fun& fun,
                               MaybeResponseHdl hdl = MaybeResponseHdl{}) {
#   ifdef CAF_LOG_LEVEL
      auto msg_str = to_string(msg);
#   endif
    CAF_LOG_TRACE(CAF_MARG(mid, integer_value) << ", msg = " << msg_str);
    auto mid = self->current_element()->mid;
    auto res = fun(self->current_element()->msg);
    CAF_LOG_DEBUG_IF(res, "actor did consume message: " << msg_str);
    CAF_LOG_DEBUG_IF(!res, "actor did ignore message: " << msg_str);
    if (!res) {
      return none;
    }
    if (res->empty()) {
      // make sure synchronous requests
      // always receive a response
      if (mid.is_request() && !mid.is_answered()) {
        CAF_LOG_WARNING("actor with ID " << self->id()
                        << " did not reply to a synchronous request message");
        auto fhdl = fetch_response_promise(self, hdl);
        if (fhdl) {
          fhdl.deliver(make_message(unit));
        }
      }
    } else {
      CAF_LOGF_DEBUG("res = " << to_string(*res));
      if (res->size() == 2
          && res->match_element(0, detail::type_nr<atom_value>::value, nullptr)
          && res->match_element(1, detail::type_nr<uint64_t>::value, nullptr)
          && res->template get_as<atom_value>(0) == atom("MESSAGE_ID")) {
        CAF_LOG_DEBUG("message handler returned a message id wrapper");
        auto id = res->template get_as<uint64_t>(1);
        auto msg_id = message_id::from_integer_value(id);
        auto ref_opt = self->sync_handler(msg_id);
        // install a behavior that calls the user-defined behavior
        // and using the result of its inner behavior as response
        if (ref_opt) {
          auto fhdl = fetch_response_promise(self, hdl);
          behavior inner = *ref_opt;
          ref_opt->assign(
            others() >> [=] {
              // inner is const inside this lambda and mutable a C++14 feature
              behavior cpy = inner;
              auto inner_res = cpy(self->last_dequeued());
              if (inner_res) {
                fhdl.deliver(*inner_res);
              }
            }
          );
        }
      } else {
        // respond by using the result of 'fun'
        CAF_LOG_DEBUG("respond via response_promise");
        auto fhdl = fetch_response_promise(self, hdl);
        if (fhdl) {
          fhdl.deliver(std::move(*res));
          // inform caller about success
          return message{};
        }
      }
    }
    return res;
  }

 private:
  // identifies 'special' messages that should not be processed normally:
  // - system messages such as EXIT (if self doesn't trap exits) and TIMEOUT
  // - expired synchronous response messages
  template <class Actor>
  msg_type filter_msg(Actor* self, mailbox_element& node) {
    const message& msg = node.msg;
    auto mid = node.mid;
    if (msg.size() == 1) {
      if (msg.match_element<exit_msg>(0)) {
        auto& em = msg.get_as<exit_msg>(0);
        CAF_REQUIRE(!mid.valid());
        // make sure to get rid of attachables if they're no longer needed
        self->unlink_from(em.source);
        if (self->trap_exit() == false) {
          if (em.reason != exit_reason::normal) {
            self->quit(em.reason);
            return msg_type::non_normal_exit;
          }
          return msg_type::normal_exit;
        }
      } else if (msg.match_element<timeout_msg>(0)) {
        auto& tm = msg.get_as<timeout_msg>(0);
        auto tid = tm.timeout_id;
        CAF_REQUIRE(!mid.valid());
        if (self->is_active_timeout(tid)) {
          return msg_type::timeout;
        }
        return self->waits_for_timeout(tid) ? msg_type::inactive_timeout
                                            : msg_type::expired_timeout;
      } else if (mid.is_response() && msg.match_element<sync_timeout_msg>(0)) {
        return msg_type::timeout_response;
      }
    }
    if (mid.is_response()) {
      return self->awaits(mid) ? msg_type::sync_response
                               : msg_type::expired_sync_response;
    }
    return msg_type::ordinary;
  }
};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_INVOKE_POLICY_HPP
