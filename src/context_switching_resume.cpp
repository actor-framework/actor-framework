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


#include "cppa/policy/context_switching_resume.hpp"
#ifndef CPPA_DISABLE_CONTEXT_SWITCHING

#include <iostream>

#include "cppa/cppa.hpp"
#include "cppa/exception.hpp"

namespace cppa {
namespace policy {

void context_switching_resume::trampoline(void* this_ptr) {
    auto _this = reinterpret_cast<blocking_untyped_actor*>(this_ptr);
    auto shut_actor_down = [_this](std::uint32_t reason) {
        if (_this->planned_exit_reason() == exit_reason::not_exited) {
            _this->planned_exit_reason(reason);
        }
        _this->on_exit();
        _this->cleanup(_this->planned_exit_reason());
    };
    try {
        _this->act();
        shut_actor_down(exit_reason::normal);
    }
    catch (actor_exited& e) {
        shut_actor_down(e.reason());
    }
    catch (...) {
        shut_actor_down(exit_reason::unhandled_exception);
    }
    std::atomic_thread_fence(std::memory_order_seq_cst);
    detail::yield(detail::yield_state::done);
}

} // namespace policy
} // namespace cppa

#else // ifdef CPPA_DISABLE_CONTEXT_SWITCHING

namespace cppa {
namespace policy {

void context_switching_resume::trampoline(void*) {
    throw std::logic_error("context_switching_resume::trampoline called");
}

} // namespace policy
} // namespace cppa

namespace cppa { int keep_compiler_happy_function() { return 42; } }

#endif // ifdef CPPA_DISABLE_CONTEXT_SWITCHING
