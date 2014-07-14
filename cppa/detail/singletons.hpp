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

#ifndef CPPA_SINGLETON_MANAGER_HPP
#define CPPA_SINGLETON_MANAGER_HPP

#include <atomic>
#include <cstddef> // size_t

#include "cppa/fwd.hpp"

namespace cppa {
namespace detail {

class abstract_singleton {

 public:

    virtual ~abstract_singleton();

    virtual void dispose() = 0;

    virtual void stop() = 0;

    virtual void initialize() = 0;

};

class singletons {

    singletons() = delete;

 public:

    static constexpr size_t max_plugin_singletons = 3;

    static constexpr size_t middleman_plugin_id = 0; // boost::actor_io

    static constexpr size_t opencl_plugin_id = 1; // for future use

    static constexpr size_t actorshell_plugin_id = 2; // for future use

    static logging* get_logger();

    static node_id get_node_id();

    static scheduler::abstract_coordinator* get_scheduling_coordinator();

    static group_manager* get_group_manager();

    static actor_registry* get_actor_registry();

    static uniform_type_info_map* get_uniform_type_info_map();

    static message_data* get_tuple_dummy();

    // usually guarded by implementation-specific singleton getter
    template<typename Factory>
    static abstract_singleton* get_plugin_singleton(size_t id, Factory f) {
        return lazy_get(get_plugin_singleton(id), f);
    }

    static void stop_singletons();

 private:

    static std::atomic<abstract_singleton*>& get_plugin_singleton(size_t id);

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
    template<typename T, typename Factory>
    static T* lazy_get(std::atomic<T*>& ptr, Factory f) {
        T* result = ptr.load();
        while (result == nullptr) {
            auto tmp = f();
            // double check if singleton is still undefined
            if (ptr.load() == nullptr) {
                tmp->initialize();
                if (ptr.compare_exchange_weak(result, tmp)) {
                    result = tmp;
                } else {
                    tmp->stop();
                    tmp->dispose();
                }
            } else {
                tmp->dispose();
            }
        }
        return result;
    }

    template<typename T>
    static T* lazy_get(std::atomic<T*>& ptr) {
        return lazy_get(ptr, [] { return T::create_singleton(); });
    }

    template<typename T>
    static void stop(std::atomic<T*>& ptr) {
        auto p = ptr.load();
        if (p) p->stop();
    }

    template<typename T>
    static void dispose(std::atomic<T*>& ptr) {
        for (;;) {
            auto p = ptr.load();
            if (p == nullptr) {
                return;
            } else if (ptr.compare_exchange_weak(p, nullptr)) {
                p->dispose();
                ptr = nullptr;
                return;
            }
        }
    }

};

} // namespace detail
} // namespace cppa

#endif // CPPA_SINGLETON_MANAGER_HPP
