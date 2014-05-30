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


#include "cppa/policy/context_switching_resume.hpp"
#ifdef CPPA_ENABLE_CONTEXT_SWITCHING

#include <iostream>

#include "cppa/cppa.hpp"
#include "cppa/exception.hpp"

namespace cppa {
namespace policy {

void context_switching_resume::trampoline(void* this_ptr) {
    CPPA_LOGF_TRACE(CPPA_ARG(this_ptr));
    auto self = reinterpret_cast<blocking_actor*>(this_ptr);
    auto shut_actor_down = [self](std::uint32_t reason) {
        if (self->planned_exit_reason() == exit_reason::not_exited) {
            self->planned_exit_reason(reason);
        }
        self->on_exit();
        self->cleanup(self->planned_exit_reason());
    };
    try {
        self->act();
        shut_actor_down(exit_reason::normal);
    }
    catch (actor_exited& e) {
        shut_actor_down(e.reason());
    }
    catch (...) {
        shut_actor_down(exit_reason::unhandled_exception);
    }
    std::atomic_thread_fence(std::memory_order_seq_cst);
    CPPA_LOGF_DEBUG("done, yield() back to execution unit");;
    detail::yield(detail::yield_state::done);
}

} // namespace policy
} // namespace cppa

#else // ifdef CPPA_ENABLE_CONTEXT_SWITCHING

namespace cppa {
namespace policy {

void context_switching_resume::trampoline(void*) {
    throw std::logic_error("context_switching_resume::trampoline called");
}

} // namespace policy
} // namespace cppa

#endif // ifdef CPPA_ENABLE_CONTEXT_SWITCHING
