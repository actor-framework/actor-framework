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


#ifndef CPPA_THREAD_BASED_ACTOR_HPP
#define CPPA_THREAD_BASED_ACTOR_HPP

#include "cppa/config.hpp"

#include <map>
#include <list>
#include <mutex>
#include <stack>
#include <atomic>
#include <vector>
#include <memory>
#include <cstdint>

#include "cppa/atom.hpp"
#include "cppa/extend.hpp"
#include "cppa/stacked.hpp"
#include "cppa/threaded.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/mailbox_based.hpp"
#include "cppa/mailbox_element.hpp"

#include "cppa/detail/receive_policy.hpp"

namespace cppa {

class self_type;
class scheduler_helper;

/**
 * @brief An actor using the blocking API running in its own thread.
 * @extends local_actor
 */
class thread_mapped_actor : public extend<local_actor,thread_mapped_actor>::
                                   with<mailbox_based,stacked,threaded> {

    typedef combined_type super;

 public:

    thread_mapped_actor();

    thread_mapped_actor(std::function<void()> fun);

    inline void initialized(bool value) { m_initialized = value; }

    virtual bool initialized() const override;

 private:

    bool m_initialized;

};

typedef intrusive_ptr<thread_mapped_actor> thread_mapped_actor_ptr;

} // namespace cppa

#endif // CPPA_THREAD_BASED_ACTOR_HPP
