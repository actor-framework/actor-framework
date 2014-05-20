/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011-2014                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef CPPA_POLICY_INVOKE_POLICY_HPP
#define CPPA_POLICY_INVOKE_POLICY_HPP

#include <list>
#include <memory>
#include <iostream>
#include <type_traits>

#include "cppa/on.hpp"
#include "cppa/logging.hpp"
#include "cppa/behavior.hpp"
#include "cppa/to_string.hpp"
#include "cppa/message_id.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/mailbox_element.hpp"
#include "cppa/system_messages.hpp"
#include "cppa/partial_function.hpp"
#include "cppa/response_promise.hpp"

#include "cppa/detail/memory.hpp"
#include "cppa/detail/matches.hpp"

#include "cppa/util/scope_guard.hpp"

namespace cppa {
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

template<receive_policy_flag X>
struct rp_flag { typedef std::integral_constant<receive_policy_flag, X> type; };

/**
 * @brief The invoke_policy <b>concept</b> class. Please note that this
 *        class is <b>not</b> implemented. It only explains the all
 *        required member function and their behavior for any resume policy.
 */
template<class Derived>
class invoke_policy {

 public:

    /**
     * @note @p node_ptr.release() is called whenever a message was
     *       handled or dropped.
     */
    template<class Actor, class Fun>
    bool invoke_message(Actor* self,
                        unique_mailbox_element_pointer& node_ptr,
                        Fun& fun,
                        message_id awaited_response) {
        if (!node_ptr) return false;
        bool result = false;
        bool reset_pointer = true;
        switch (handle_message(self, node_ptr.get(), fun, awaited_response)) {
            case hm_msg_handled: {
                result = true;
                break;
            }
            case hm_drop_msg: {
                break;
            }
            case hm_cache_msg: {
                reset_pointer = false;
                break;
            }
            case hm_skip_msg: {
                // "received" a marked node
                reset_pointer = false;
                break;
            }
        }
        if (reset_pointer) node_ptr.reset();
        return result;
    }

    typedef typename rp_flag<rp_nestable>::type nestable;

    typedef typename rp_flag<rp_sequential>::type sequential;

 private:

    std::list<std::unique_ptr<mailbox_element, detail::disposer> > m_cache;

    inline void handle_timeout(partial_function&) {
        CPPA_CRITICAL("handle_timeout(partial_function&)");
    }

    enum class msg_type {
        normal_exit,            // an exit message with normal exit reason
        non_normal_exit,        // an exit message with abnormal exit reason
        expired_timeout,        // an 'old & obsolete' timeout
        inactive_timeout,       // a currently inactive timeout
        expired_sync_response,  // a sync response that already timed out
        timeout,                // triggers currently active timeout
        timeout_response,       // triggers timeout of a sync message
        ordinary,               // an asynchronous message or sync. request
        sync_response           // a synchronous response
    };


    // identifies 'special' messages that should not be processed normally:
    // - system messages such as EXIT (if self doesn't trap exits) and TIMEOUT
    // - expired synchronous response messages

    template<class Actor>
    msg_type filter_msg(Actor* self, mailbox_element* node) {
        const any_tuple& msg = node->msg;
        auto mid = node->mid;
        auto& arr = detail::static_types_array<exit_msg,
                                               timeout_msg,
                                               sync_timeout_msg>::arr;
        if (msg.size() == 1) {
            if (msg.type_at(0) == arr[0]) {
                auto& em = msg.get_as<exit_msg>(0);
                CPPA_REQUIRE(!mid.valid());
                if (self->trap_exit() == false) {
                    if (em.reason != exit_reason::normal) {
                        self->quit(em.reason);
                        return msg_type::non_normal_exit;
                    }
                    return msg_type::normal_exit;
                }
            }
            else if (msg.type_at(0) == arr[1]) {
                auto& tm = msg.get_as<timeout_msg>(0);
                auto tid = tm.timeout_id;
                CPPA_REQUIRE(!mid.valid());
                if (self->is_active_timeout(tid)) return msg_type::timeout;
                return self->waits_for_timeout(tid) ? msg_type::inactive_timeout
                                                    : msg_type::expired_timeout;
            }
            else if (msg.type_at(0) == arr[2] && mid.is_response()) {
                return msg_type::timeout_response;
            }
        }
        if (mid.is_response()) {
            return (self->awaits(mid)) ? msg_type::sync_response
                                       : msg_type::expired_sync_response;
        }
        return msg_type::ordinary;
    }

 public:

    template<class Actor>
    inline response_promise fetch_response_promise(Actor* cl, int) {
        return cl->make_response_promise();
    }

    template<class Actor>
    inline response_promise fetch_response_promise(Actor*, response_promise& hdl) {
        return std::move(hdl);
    }

    // - extracts response message from handler
    // - returns true if fun was successfully invoked
    template<class Actor, class Fun, class MaybeResponseHandle = int>
    optional<any_tuple> invoke_fun(Actor* self,
                                   any_tuple& msg,
                                   message_id& mid,
                                   Fun& fun,
                                   MaybeResponseHandle hdl = MaybeResponseHandle{}) {
#       if CPPA_LOG_LEVEL >= CPPA_POLICY_DEBUG
        auto msg_str = to_string(msg);
#       endif
        auto res = fun(msg); // might change mid
        CPPA_LOG_DEBUG_IF(res, "actor did consume message: " << msg_str);
        CPPA_LOG_DEBUG_IF(!res, "actor did ignore message: " << msg_str);
        if (res) {
            //message_header hdr{self, sender, mid.is_request() ? mid.response_id()
            //                                                    : message_id{}};
            if (res->empty()) {
                // make sure synchronous requests
                // always receive a response
                if (mid.is_request() && !mid.is_answered()) {
                    CPPA_LOG_WARNING("actor with ID " << self->id()
                                     << " did not reply to a "
                                        "synchronous request message");
                    auto fhdl = fetch_response_promise(self, hdl);
                    if (fhdl) fhdl.deliver(make_any_tuple(unit));
                }
            } else {
                if (   detail::matches<atom_value, std::uint64_t>(*res)
                    && res->template get_as<atom_value>(0) == atom("MESSAGE_ID")) {
                    CPPA_LOG_DEBUG("message handler returned a "
                                   "message id wrapper");
                    auto id = res->template get_as<std::uint64_t>(1);
                    auto msg_id = message_id::from_integer_value(id);
                    auto ref_opt = self->sync_handler(msg_id);
                    // calls self->response_promise() if hdl is a dummy
                    // argument, forwards hdl otherwise to reply to the
                    // original request message
                    auto fhdl = fetch_response_promise(self, hdl);
                    if (ref_opt) {
                        behavior cpy = *ref_opt;
                        *ref_opt = cpy.add_continuation(
                            [=](any_tuple& intermediate) -> optional<any_tuple> {
                                if (!intermediate.empty()) {
                                    // do no use lamba expresion type to
                                    // avoid recursive template instantiaton
                                    behavior::continuation_fun f2 = [=](any_tuple& m) -> optional<any_tuple> {
                                        return std::move(m);
                                    };
                                    auto mutable_mid = mid;
                                    // recursively call invoke_fun on the
                                    // result to correctly handle stuff like
                                    // sync_send(...).then(...).then(...)...
                                    return invoke_fun(self,
                                                      intermediate,
                                                      mutable_mid,
                                                      f2,
                                                      fhdl);
                                }
                                return none;
                            }
                        );
                    }
                    // reset res to prevent "outer" invoke_fun
                    // from handling the result again
                    res->reset();
                } else {
                    // respond by using the result of 'fun'
                    CPPA_LOG_DEBUG("respond via response_promise");
                    auto fhdl = fetch_response_promise(self, hdl);
                    if (fhdl) {
                        fhdl.deliver(std::move(*res));
                        // inform caller about success
                        return any_tuple{};
                    }
                }
            }
            return res;
        }
        // fun did not return a value => no match
        return none;
    }

    // the workflow of handle_message (hm) is as follows:
    // - should_skip? if yes: return hm_skip_msg
    // - msg is ordinary message? if yes:
    //   - begin(...) -> prepares a self for message handling
    //   - self could process message?
    //     - yes: cleanup()
    //     - no: revert(...) -> set self back to state it had before begin()

    template<class Actor, class Fun>
    handle_message_result handle_message(Actor* self,
                                         mailbox_element* node,
                                         Fun& fun,
                                         message_id awaited_response) {
        bool handle_sync_failure_on_mismatch = true;
        if (dptr()->hm_should_skip(node)) {
            return hm_skip_msg;
        }
        switch (this->filter_msg(self, node)) {
            case msg_type::normal_exit: {
                CPPA_LOG_DEBUG("dropped normal exit signal");
                return hm_drop_msg;
            }
            case msg_type::expired_sync_response: {
                CPPA_LOG_DEBUG("dropped expired sync response");
                return hm_drop_msg;
            }
            case msg_type::expired_timeout: {
                CPPA_LOG_DEBUG("dropped expired timeout message");
                return hm_drop_msg;
            }
            case msg_type::inactive_timeout: {
                CPPA_LOG_DEBUG("skipped inactive timeout message");
                return hm_skip_msg;
            }
            case msg_type::non_normal_exit: {
                CPPA_LOG_DEBUG("handled non-normal exit signal");
                // this message was handled
                // by calling self->quit(...)
                return hm_msg_handled;
            }
            case msg_type::timeout: {
                CPPA_LOG_DEBUG("handle timeout message");
                auto& tm = node->msg.get_as<timeout_msg>(0);
                self->handle_timeout(fun, tm.timeout_id);
                if (awaited_response.valid()) {
                    self->mark_arrived(awaited_response);
                    self->remove_handler(awaited_response);
                }
                return hm_msg_handled;
            }
            case msg_type::timeout_response: {
                handle_sync_failure_on_mismatch = false;
                CPPA_ANNOTATE_FALLTHROUGH;
            }
            case msg_type::sync_response: {
                CPPA_LOG_DEBUG("handle as synchronous response: "
                               << CPPA_POLICY_TARG(node->msg, to_string) << ", "
                               << CPPA_POLICY_MARG(node->mid, integer_value) << ", "
                               << CPPA_POLICY_MARG(awaited_response, integer_value));
                if (awaited_response.valid() && node->mid == awaited_response) {
                    auto previous_node = dptr()->hm_begin(self, node);
                    auto res = invoke_fun(self,
                                          node->msg,
                                          node->mid,
                                          fun);
                    if (!res && handle_sync_failure_on_mismatch) {
                        CPPA_LOG_WARNING("sync failure occured in actor "
                                         << "with ID " << self->id());
                        self->handle_sync_failure();
                    }
                    self->mark_arrived(awaited_response);
                    self->remove_handler(awaited_response);
                    dptr()->hm_cleanup(self, previous_node);
                    return hm_msg_handled;
                }
                return hm_cache_msg;
            }
            case msg_type::ordinary: {
                if (!awaited_response.valid()) {
                    auto previous_node = dptr()->hm_begin(self, node);
                    auto res = invoke_fun(self,
                                          node->msg,
                                          node->mid,
                                          fun);
                    if (res) {
                        dptr()->hm_cleanup(self, previous_node);
                        return hm_msg_handled;
                    }
                    // no match (restore self members)
                    dptr()->hm_revert(self, previous_node);
                }
                CPPA_LOG_DEBUG_IF(awaited_response.valid(),
                                  "ignored message; await response: "
                                  << awaited_response.integer_value());
                return hm_cache_msg;
            }
        }
        // should be unreachable
        CPPA_CRITICAL("invalid message type");
    }

    Derived* dptr() {
        return static_cast<Derived*>(this);
    }

};

} // namespace policy
} // namespace cppa

#endif // CPPA_POLICY_INVOKE_POLICY_HPP
