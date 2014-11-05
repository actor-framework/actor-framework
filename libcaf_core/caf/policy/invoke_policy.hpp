/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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
#include "caf/message_handler.hpp"
#include "caf/response_promise.hpp"

#include "caf/detail/memory.hpp"
#include "caf/detail/logging.hpp"
#include "caf/detail/scope_guard.hpp"

namespace caf {
namespace policy {

enum receive_policy_flag {
  // receives can be nested
  rp_nestable,
  // receives are guaranteed to be sequential
  rp_sequential
};

enum handle_message_result {
  hm_skip_msg,
  hm_drop_msg,
  hm_cache_msg,
  hm_msg_handled
};

template <receive_policy_flag X>
struct rp_flag {
  using type = std::integral_constant<receive_policy_flag, X>;
};

/**
 * Base class for invoke policies.
 */
template <class Derived>
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

  /**
   * @note `node_ptr`.release() is called whenever a message was
   *     handled or dropped.
   */
  template <class Actor, class Fun>
  bool invoke_message(Actor* self, unique_mailbox_element_pointer& node_ptr,
                      Fun& fun, message_id awaited_response) {
    if (!node_ptr) {
      return false;
    }
    CAF_LOG_TRACE("");
    switch (handle_message(self, node_ptr.get(), fun, awaited_response)) {
      case hm_msg_handled:
        node_ptr.reset();
        return true;
      case hm_cache_msg:
      case hm_skip_msg:
        // do not reset ptr (not handled => cache or skip)
        return false;
      default: // drop message
        node_ptr.reset();
        return false;
    }
  }

  using nestable = typename rp_flag<rp_nestable>::type;

  using sequential = typename rp_flag<rp_sequential>::type;

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
  optional<message> invoke_fun(Actor* self, message& msg, message_id& mid,
                               Fun& fun,
                               MaybeResponseHdl hdl = MaybeResponseHdl{}) {
#   ifdef CAF_LOG_LEVEL
      auto msg_str = to_string(msg);
#   endif
    CAF_LOG_TRACE(CAF_MARG(mid, integer_value) << ", msg = " << msg_str);
    auto res = fun(msg); // might change mid
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
      if (res->template has_types<atom_value, uint64_t>()
          && res->template get_as<atom_value>(0) == atom("MESSAGE_ID")) {
        CAF_LOG_DEBUG("message handler returned a message id wrapper");
        auto id = res->template get_as<uint64_t>(1);
        auto msg_id = message_id::from_integer_value(id);
        auto ref_opt = self->sync_handler(msg_id);
        // calls self->response_promise() if hdl is a dummy
        // argument, forwards hdl otherwise to reply to the
        // original request message
        auto fhdl = fetch_response_promise(self, hdl);
        if (ref_opt) {
          behavior cpy = *ref_opt;
          *ref_opt =
            cpy.add_continuation([=](message & intermediate)
                         ->optional<message> {
              if (!intermediate.empty()) {
                // do no use lamba expresion type to
                // avoid recursive template instantiaton
                behavior::continuation_fun f2 = [=](
                  message & m)->optional<message> {
                  return std::move(m);

                };
                auto mutable_mid = mid;
                // recursively call invoke_fun on the
                // result to correctly handle stuff like
                // sync_send(...).then(...).then(...)...
                return this->invoke_fun(self, intermediate,
                             mutable_mid, f2, fhdl);
              }
              return none;
            });
        }
        // reset res to prevent "outer" invoke_fun
        // from handling the result again
        res->reset();
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

  // the workflow of handle_message (hm) is as follows:
  // - should_skip? if yes: return hm_skip_msg
  // - msg is ordinary message? if yes:
  //   - begin(...) -> prepares a self for message handling
  //   - self could process message?
  //   - yes: cleanup()
  //   - no: revert(...) -> set self back to state it had before begin()

  template <class Actor, class Fun>
  handle_message_result handle_message(Actor* self, mailbox_element* node,
                                       Fun& fun, message_id awaited_response) {
    bool handle_sync_failure_on_mismatch = true;
    if (dptr()->hm_should_skip(node)) {
      return hm_skip_msg;
    }
    switch (this->filter_msg(self, node)) {
      case msg_type::normal_exit:
        CAF_LOG_DEBUG("dropped normal exit signal");
        return hm_drop_msg;
      case msg_type::expired_sync_response:
        CAF_LOG_DEBUG("dropped expired sync response");
        return hm_drop_msg;
      case msg_type::expired_timeout:
        CAF_LOG_DEBUG("dropped expired timeout message");
        return hm_drop_msg;
      case msg_type::inactive_timeout:
        CAF_LOG_DEBUG("skipped inactive timeout message");
        return hm_skip_msg;
      case msg_type::non_normal_exit:
        CAF_LOG_DEBUG("handled non-normal exit signal");
        // this message was handled
        // by calling self->quit(...)
        return hm_msg_handled;
      case msg_type::timeout: {
        CAF_LOG_DEBUG("handle timeout message");
        auto& tm = node->msg.get_as<timeout_msg>(0);
        self->handle_timeout(fun, tm.timeout_id);
        if (awaited_response.valid()) {
          self->mark_arrived(awaited_response);
          self->remove_handler(awaited_response);
        }
        return hm_msg_handled;
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
          auto previous_node = dptr()->hm_begin(self, node);
          auto res = invoke_fun(self, node->msg, node->mid, fun);
          if (!res && handle_sync_failure_on_mismatch) {
            CAF_LOG_WARNING("sync failure occured in actor "
                     << "with ID " << self->id());
            self->handle_sync_failure();
          }
          self->mark_arrived(awaited_response);
          self->remove_handler(awaited_response);
          dptr()->hm_cleanup(self, previous_node);
          return hm_msg_handled;
        }
        return hm_cache_msg;
      case msg_type::ordinary:
        if (!awaited_response.valid()) {
          auto previous_node = dptr()->hm_begin(self, node);
          auto res = invoke_fun(self, node->msg, node->mid, fun);
          if (res) {
            dptr()->hm_cleanup(self, previous_node);
            return hm_msg_handled;
          }
          // no match (restore self members)
          dptr()->hm_revert(self, previous_node);
        }
        CAF_LOG_DEBUG_IF(awaited_response.valid(),
                  "ignored message; await response: "
                    << awaited_response.integer_value());
        return hm_cache_msg;
    }
    // should be unreachable
    CAF_CRITICAL("invalid message type");
  }

 protected:
  Derived* dptr() {
    return static_cast<Derived*>(this);
  }

 private:
  void handle_timeout(message_handler&) {
    CAF_CRITICAL("handle_timeout(message_handler&)");
  }

  // identifies 'special' messages that should not be processed normally:
  // - system messages such as EXIT (if self doesn't trap exits) and TIMEOUT
  // - expired synchronous response messages

  template <class Actor>
  msg_type filter_msg(Actor* self, mailbox_element* node) {
    const message& msg = node->msg;
    auto mid = node->mid;
    if (msg.size() == 1) {
      if (msg.type_at(0)->equal_to(typeid(exit_msg))) {
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
      } else if (msg.type_at(0)->equal_to(typeid(timeout_msg))) {
        auto& tm = msg.get_as<timeout_msg>(0);
        auto tid = tm.timeout_id;
        CAF_REQUIRE(!mid.valid());
        if (self->is_active_timeout(tid)) {
          return msg_type::timeout;
        }
        return self->waits_for_timeout(tid) ? msg_type::inactive_timeout
                                            : msg_type::expired_timeout;
      } else if (msg.type_at(0)->equal_to(typeid(sync_timeout_msg))
                 && mid.is_response()) {
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
