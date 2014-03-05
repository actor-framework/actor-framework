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


#ifndef CPPA_SINGLETON_MANAGER_HPP
#define CPPA_SINGLETON_MANAGER_HPP

#include <atomic>

namespace cppa {

class logging;
class scheduler;

} // namespace cppa

namespace cppa { namespace io { class middleman; } }

namespace cppa { namespace opencl { class opencl_metainfo; } }

namespace cppa { namespace windows { class windows_tcp; } }

namespace cppa { namespace detail {

class empty_tuple;
class group_manager;
class abstract_tuple;
class actor_registry;
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

    static io::middleman* get_middleman();

    static uniform_type_info_map* get_uniform_type_info_map();

    static abstract_tuple* get_tuple_dummy();

    static empty_tuple* get_empty_tuple();

    static opencl::opencl_metainfo* get_opencl_metainfo();

    static windows::windows_tcp* get_windows_tcp();

 private:

    /*
     * @brief Type @p T has to provide: <tt>static T* create_singleton()</tt>,
     *        <tt>void initialize()</tt>, <tt>void destroy()</tt>,
     *        and <tt>dispose()</tt>.
     * The constructor of T shall be lightweigt, since more than one object
     * might get constructed initially.
     * <tt>dispose()</tt> is called on objects with failed CAS operation.
     * <tt>initialize()</tt> is called on objects with succeeded CAS operation.
     * <tt>destroy()</tt> is called during shutdown on initialized objects.
     *
     * Both <tt>dispose</tt> and <tt>destroy</tt> must delete the object
     * eventually.
     */
    template<typename T>
    static T* lazy_get(std::atomic<T*>& ptr) {
        T* result = ptr.load();
        while (result == nullptr) {
            auto tmp = T::create_singleton();
            // double check if singleton is still undefined
            if (ptr.load() == nullptr) {
                tmp->initialize();
                if (ptr.compare_exchange_weak(result, tmp)) {
                    result = tmp;
                }
                else tmp->destroy();
            }
            else tmp->dispose();
        }
        return result;
    }

    template<typename T>
    static void destroy(std::atomic<T*>& ptr) {
        for (;;) {
            auto p = ptr.load();
            if (p == nullptr) {
                return;
            }
            else if (ptr.compare_exchange_weak(p, nullptr)) {
                p->destroy();
                ptr = nullptr;
                return;
            }
        }
    }

};

} } // namespace cppa::detail

#endif // CPPA_SINGLETON_MANAGER_HPP
