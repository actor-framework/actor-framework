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
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
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


#ifndef CPPA_SINGLETON_MANAGER_HPP
#define CPPA_SINGLETON_MANAGER_HPP

#include <atomic>

namespace cppa {

class scheduler;
class msg_content;

} // namespace cppa

namespace cppa { namespace network { class middleman; } }

namespace cppa { namespace detail {

class logging;
class empty_tuple;
class group_manager;
class abstract_tuple;
class actor_registry;
class decorated_names_map;
class uniform_type_info_map;

class singleton_manager {

    singleton_manager() = delete;

 public:

    static void shutdown();

    static logging* get_logger();

    static scheduler* get_scheduler();

    static bool set_scheduler(scheduler*);

    static group_manager* get_group_manager();

    static actor_registry* get_actor_registry();

    // created on-the-fly on a successfull call to set_scheduler()
    static network::middleman* get_middleman();

    static uniform_type_info_map* get_uniform_type_info_map();

    static abstract_tuple* get_tuple_dummy();

    static empty_tuple* get_empty_tuple();

    static decorated_names_map* get_decorated_names_map();

 private:

    template<typename T>
    static void stop_and_kill(std::atomic<T*>& ptr) {
        for (;;) {
            auto p = ptr.load();
            if (p == nullptr) {
                return;
            }
            else if (ptr.compare_exchange_weak(p, nullptr)) {
                p->stop();
                delete p;
                ptr = nullptr;
                return;
            }
        }
    }

};

} } // namespace cppa::detail

#endif // CPPA_SINGLETON_MANAGER_HPP
