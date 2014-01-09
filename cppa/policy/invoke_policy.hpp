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
 * Copyright (C) 2011-2013                                                    *
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


#ifndef CPPA_INVOKE_POLICY_HPP
#define CPPA_INVOKE_POLICY_HPP

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
#include "cppa/partial_function.hpp"
#include "cppa/response_promise.hpp"

#include "cppa/util/dptr.hpp"
#include "cppa/detail/memory.hpp"
#include "cppa/detail/matches.hpp"

#include "cppa/util/scope_guard.hpp"

namespace cppa { namespace policy {

enum receive_policy_flag {
    // receives can be nested
    rp_nestable,
    // receives are guaranteed to be sequential
    rp_sequential
};

enum handle_message_result {
    hm_timeout_msg,
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

    typedef mailbox_element* pointer;
    typedef std::unique_ptr<mailbox_element, detail::disposer> smart_pointer;

    template<class Actor, class Fun>
    bool invoke_from_cache(Actor* self,
                           Fun& fun,
                           message_id awaited_response = message_id{}) {
        auto i = m_cache.begin();
        auto e = m_cache.end();
        while (i != e) {
            switch (handle_message(self, i->get(), fun, awaited_response)) {
                case hm_msg_handled: {
                    m_cache.erase(i);
                    return true;
                }
                case hm_drop_msg: {
                    i = m_cache.erase(i);
                    break;
                }
                case hm_skip_msg:
                case hm_cache_msg: {
                    ++i;
                    break;
                }
                default: {
                    CPPA_CRITICAL("illegal result of handle_message");
                }
            }
        }
        return false;
    }

    inline void add_to_cache(pointer node_ptr) {
        m_cache.emplace_back(std::move(node_ptr));
    }

    template<class Actor, class Fun>
    bool invoke(Actor* self,
                pointer node_ptr,
                Fun& fun,
                message_id awaited_response = message_id()) {
        smart_pointer node(node_ptr);
        switch (handle_message(self, node.get(), fun, awaited_response)) {
            case hm_msg_handled: {
                return true;
            }
            case hm_drop_msg: {
                break;
            }
            case hm_cache_msg: {
                m_cache.emplace_back(std::move(node));
                break;
            }
            case hm_skip_msg: {
                CPPA_CRITICAL("received a marked node");
            }
            default: {
                CPPA_CRITICAL("illegal result of handle_message");
            }
        }
        return false;
    }

    template<class Actor, class FunOrBehavior>
    inline void receive_wo_timeout(Actor* self, FunOrBehavior& fun) {
        if (!invoke_from_cache(self, fun)) {
            while (!invoke(self, self->await_message(), fun)) { }
        }
    }

    template<class Actor>
    void receive(Actor* self, partial_function& fun) {
        receive_wo_timeout(self, fun);
    }

    template<class Actor>
    void receive(Actor* self, behavior& bhvr) {
        if (!bhvr.timeout().valid()) {
            receive_wo_timeout(self, bhvr);
        }
        else if (!invoke_from_cache(self, bhvr)) {
            if (bhvr.timeout().is_zero()) {
                pointer e = nullptr;
                while ((e = self->try_pop()) != nullptr) {
                    CPPA_REQUIRE(e->marked == false);
                    if (invoke(self, e, bhvr)) {
                        return; // done
                    }
                }
                dptr()->handle_timeout(self, bhvr);
            }
            else {
                auto timeout = self->init_timeout(bhvr.timeout());
                pointer e = nullptr;
                while ((e = self->await_message(timeout)) != nullptr) {
                    CPPA_REQUIRE(e->marked == false);
                    if (invoke(self, e, bhvr)) {
                        return; // done
                    }
                }
                dptr()->handle_timeout(self, bhvr);
            }
        }
    }

    template<class Actor>
    void receive(Actor* self, behavior& bhvr, message_id mid) {
        CPPA_REQUIRE(mid.is_response());
        if (!invoke_from_cache(self, bhvr, mid)) {
            if (bhvr.timeout().valid()) {
                CPPA_REQUIRE(bhvr.timeout().is_zero() == false);
                auto timeout = self->init_timeout(bhvr.timeout());
                pointer e = nullptr;
                while ((e = self->await_message(timeout)) != nullptr) {
                    CPPA_REQUIRE(e->marked == false);
                    if (invoke(self, e, bhvr, mid)) {
                        return; // done
                    }
                }
                dptr()->handle_timeout(self, bhvr);
            }
            else while (!invoke(self, self->await_message(), bhvr, mid)) { }
        }
    }

    template<class Actor>
    mailbox_element* fetch_message(Actor* self) {
        return self->await_message();
    }

    typedef typename rp_flag<rp_nestable>::type nestable;
    typedef typename rp_flag<rp_sequential>::type sequential;

 private:

    std::list<std::unique_ptr<mailbox_element, detail::disposer> > m_cache;

    template<class Actor>
    inline void handle_timeout(Actor*, partial_function&) {
        CPPA_CRITICAL("handle_timeout(partial_function&)");
    }

    enum filter_result {
        normal_exit_signal,
        non_normal_exit_signal,
        expired_timeout_message,
        expired_sync_response,
        timeout_message,
        timeout_response_message,
        ordinary_message,
        sync_response
    };


    // identifies 'special' messages that should not be processed normally:
    // - system messages such as EXIT (if self doesn't trap exits) and TIMEOUT
    // - expired synchronous response messages

    template<class Actor>
    filter_result filter_msg(Actor* self, pointer node) {
        const any_tuple& msg = node->msg;
        auto mid = node->mid;
        auto& arr = detail::static_types_array<atom_value, std::uint32_t>::arr;
        if (   msg.size() == 2
            && msg.type_at(0) == arr[0]
            && msg.type_at(1) == arr[1]) {
            auto v0 = msg.get_as<atom_value>(0);
            auto v1 = msg.get_as<std::uint32_t>(1);
            if (v0 == atom("EXIT")) {
                CPPA_REQUIRE(!mid.valid());
                if (self->trap_exit() == false) {
                    if (v1 != exit_reason::normal) {
                        self->quit(v1);
                        return non_normal_exit_signal;
                    }
                    return normal_exit_signal;
                }
            }
            else if (v0 == atom("SYNC_TOUT")) {
                CPPA_REQUIRE(!mid.valid());
                return dptr()->waits_for_timeout(v1) ? timeout_message
                                                     : expired_timeout_message;
            }
        }
        else if (   msg.size() == 1
                 && msg.type_at(0) == arr[0]
                 && msg.get_as<atom_value>(0) == atom("TIMEOUT")
                 && mid.is_response()) {
            return timeout_response_message;
        }
        if (mid.is_response()) {
            return (self->awaits(mid)) ? sync_response
                                                : expired_sync_response;
        }
        return ordinary_message;
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
        auto res = fun(msg); // might change mid
        CPPA_LOG_DEBUG_IF(res, "actor did consume message");
        CPPA_LOG_DEBUG_IF(!res, "actor did ignore message");
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
                    if (fhdl) fhdl.deliver(make_any_tuple(atom("VOID")));
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
                        auto& ref = *ref_opt;
                        // copy original behavior
                        behavior cpy = ref;
                        ref = cpy.add_continuation(
                            [=](any_tuple& intermediate) -> optional<any_tuple> {
                                if (!intermediate.empty()) {
                                    // do no use lamba expresion type to
                                    // avoid recursive template intantiaton
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
                    if (fhdl) fhdl.deliver(std::move(*res));
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
                                         pointer node,
                                         Fun& fun,
                                         message_id awaited_response) {
        bool handle_sync_failure_on_mismatch = true;
        if (dptr()->hm_should_skip(node)) {
            return hm_skip_msg;
        }
        switch (this->filter_msg(self, node)) {
            default: {
                CPPA_LOG_ERROR("illegal filter result");
                CPPA_CRITICAL("illegal filter result");
            }
            case normal_exit_signal: {
                CPPA_LOG_DEBUG("dropped normal exit signal");
                return hm_drop_msg;
            }
            case expired_sync_response: {
                CPPA_LOG_DEBUG("dropped expired sync response");
                return hm_drop_msg;
            }
            case expired_timeout_message: {
                CPPA_LOG_DEBUG("dropped expired timeout message");
                return hm_drop_msg;
            }
            case non_normal_exit_signal: {
                CPPA_LOG_DEBUG("handled non-normal exit signal");
                // this message was handled
                // by calling self->quit(...)
                return hm_msg_handled;
            }
            case timeout_message: {
                CPPA_LOG_DEBUG("handle timeout message");
                dptr()->handle_timeout(self, fun);
                if (awaited_response.valid()) {
                    self->mark_arrived(awaited_response);
                    self->remove_handler(awaited_response);
                }
                return hm_msg_handled;
            }
            case timeout_response_message: {
                handle_sync_failure_on_mismatch = false;
                // fall through
            }
            case sync_response: {
                CPPA_LOG_DEBUG("handle as synchronous response: "
                               << CPPA_TARG(node->msg, to_string) << ", "
                               << CPPA_MARG(node->mid, integer_value) << ", "
                               << CPPA_MARG(awaited_response, integer_value));
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
            case ordinary_message: {
                CPPA_LOG_DEBUG("handle as ordinary message: "
                               << CPPA_TARG(node->msg, to_string));
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
    }

    Derived* dptr() {
        return static_cast<Derived*>(this);
    }

};

} } // namespace cppa::policy

#endif // CPPA_INVOKE_POLICY_HPP
