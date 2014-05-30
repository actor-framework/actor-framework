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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_POLICY_NESTABLE_INVOKE_HPP
#define CPPA_POLICY_NESTABLE_INVOKE_HPP

#include <mutex>
#include <chrono>
#include <condition_variable>

#include "cppa/exit_reason.hpp"
#include "cppa/mailbox_element.hpp"

#include "cppa/detail/sync_request_bouncer.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

#include "cppa/policy/invoke_policy.hpp"

namespace cppa {
namespace policy {

class nestable_invoke : public invoke_policy<nestable_invoke> {

 public:

    inline bool hm_should_skip(mailbox_element* node) {
        return node->marked;
    }

    template<class Actor>
    inline mailbox_element* hm_begin(Actor* self, mailbox_element* node) {
        auto previous = self->current_node();
        self->current_node(node);
        self->push_timeout();
        node->marked = true;
        return previous;
    }

    template<class Actor>
    inline void hm_cleanup(Actor* self, mailbox_element* previous) {
        self->current_node()->marked = false;
        self->current_node(previous);
    }

    template<class Actor>
    inline void hm_revert(Actor* self, mailbox_element* previous) {
        self->current_node()->marked = false;
        self->current_node(previous);
        self->pop_timeout();
    }

};

} // namespace policy
} // namespace cppa

#endif // CPPA_POLICY_NESTABLE_INVOKE_HPP
