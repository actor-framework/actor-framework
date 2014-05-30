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


#ifndef NO_RESUME_HPP
#define NO_RESUME_HPP

#include <chrono>
#include <utility>

#include "cppa/exception.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/policy/resume_policy.hpp"

namespace cppa { namespace detail { struct cs_thread; } }


namespace cppa {
namespace policy {

// this policy simply forwards calls to @p await_data to the scheduling
// policy and throws an exception whenever @p resume is called;
// it intentionally works only with the no_scheduling policy
class no_resume {

 public:

    template<class Base, class Derived>
    struct mixin : Base {

        template<typename... Ts>
        mixin(Ts&&... args)
                : Base(std::forward<Ts>(args)...)
                , m_hidden(true) { }

        inline void attach_to_scheduler() {
            this->ref();
        }

        inline void detach_from_scheduler() {
            this->deref();
        }

        inline resumable::resume_result resume(detail::cs_thread*,
                                               execution_unit*) {
            auto done_cb = [=](std::uint32_t reason) {
                this->planned_exit_reason(reason);
                this->on_exit();
                this->cleanup(reason);
            };
            try {
                this->act();
                done_cb(exit_reason::normal);
            }
            catch (actor_exited& e) {
                done_cb(e.reason());
            }
            catch (...) {
                done_cb(exit_reason::unhandled_exception);
            }
            return resumable::done;
        }

        bool m_hidden;

    };

    template<class Actor>
    void await_ready(Actor* self) {
        self->await_data();
    }

};

} // namespace policy
} // namespace cppa

#endif // NO_RESUME_HPP
