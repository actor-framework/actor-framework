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


#ifndef CPPA_SCOPED_ACTOR_HPP
#define CPPA_SCOPED_ACTOR_HPP

#include "cppa/behavior.hpp"
#include "cppa/blocking_actor.hpp"

namespace cppa {

/**
 * @brief A scoped handle to a blocking actor.
 */
class scoped_actor {

 public:

    scoped_actor();

    scoped_actor(const scoped_actor&) = delete;

    explicit scoped_actor(bool hide_actor);

    ~scoped_actor();

    inline blocking_actor* operator->() const {
        return m_self.get();
    }

    inline blocking_actor& operator*() const {
        return *m_self;
    }

    inline blocking_actor* get() const {
        return m_self.get();
    }

    operator channel() const {
        return get();
    }

    operator actor() const {
        return get();
    }

    operator actor_addr() const {
        return get()->address();
    }

    inline actor_addr address() const {
        return get()->address();
    }

 private:

    void init(bool hide_actor);

    bool m_hidden;
    actor_id m_prev; // used for logging/debugging purposes only
    intrusive_ptr<blocking_actor> m_self;

};

} // namespace cppa

#endif // CPPA_SCOPED_ACTOR_HPP
