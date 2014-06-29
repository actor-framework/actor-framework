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

#ifndef CPPA_THREADLESS_HPP
#define CPPA_THREADLESS_HPP

#include "cppa/atom.hpp"
#include "cppa/behavior.hpp"
#include "cppa/duration.hpp"

#include "cppa/policy/invoke_policy.hpp"

namespace cppa {
namespace policy {

/**
 * @brief An actor that is scheduled or otherwise managed.
 */
class sequential_invoke : public invoke_policy<sequential_invoke> {

 public:

    inline bool hm_should_skip(mailbox_element*) { return false; }

    template<class Actor>
    inline mailbox_element* hm_begin(Actor* self, mailbox_element* node) {
        auto previous = self->current_node();
        self->current_node(node);
        return previous;
    }

    template<class Actor>
    inline void hm_cleanup(Actor* self, mailbox_element*) {
        self->current_node(self->dummy_node());
    }

    template<class Actor>
    inline void hm_revert(Actor* self, mailbox_element* previous) {
        self->current_node(previous);
    }

};

} // namespace policy
} // namespace cppa

#endif // CPPA_THREADLESS_HPP
