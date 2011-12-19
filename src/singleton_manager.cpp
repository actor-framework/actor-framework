#include <atomic>
#include <iostream>

#include "cppa/any_tuple.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/exception.hpp"

#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/empty_tuple.hpp"
#include "cppa/detail/group_manager.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/network_manager.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"
#include "cppa/detail/uniform_type_info_map.hpp"
#include "cppa/detail/converted_thread_context.hpp"

namespace {

using namespace cppa;
using namespace cppa::detail;

struct singleton_container
{

    std::atomic<scheduler*> m_scheduler;
    network_manager* m_network_manager;

    uniform_type_info_map* m_uniform_type_info_map;
    actor_registry* m_actor_registry;
    group_manager* m_group_manager;
    empty_tuple* m_empty_tuple;

    singleton_container() : m_scheduler(nullptr), m_network_manager(nullptr)
    {
        m_uniform_type_info_map = new uniform_type_info_map;
        m_actor_registry = new actor_registry;
        m_group_manager = new group_manager;
        m_empty_tuple = new empty_tuple;
        m_empty_tuple->ref();
    }

    ~singleton_container()
    {
        // disconnect all peers
        if (m_network_manager)
        {
            m_network_manager->stop();
            delete m_network_manager;
        }
        // run actor specific cleanup if needed
        if (self.unchecked() != nullptr)
        {
            try
            {
                self.unchecked()->quit(exit_reason::normal);
            }
            catch (actor_exited&)
            {
            }
        }
        // wait for all running actors to quit
        m_actor_registry->await_running_count_equal(0);
        // shutdown scheduler
        // TODO: figure out why the ... Mac OS dies with a segfault here
#       ifndef __APPLE__
        auto s = m_scheduler.load();
        if (s)
        {
            s->stop();
            delete s;
        }
#       endif
        // it's safe now to delete all other singletons
        delete m_group_manager;
        if (!m_empty_tuple->deref()) delete m_empty_tuple;
        delete m_actor_registry;
        delete m_uniform_type_info_map;
    }

};

singleton_container s_container;

} // namespace <anonymous>

namespace cppa { namespace detail {

actor_registry* singleton_manager::get_actor_registry()
{
    return s_container.m_actor_registry;
}

uniform_type_info_map* singleton_manager::get_uniform_type_info_map()
{
    return s_container.m_uniform_type_info_map;
}

group_manager* singleton_manager::get_group_manager()
{
    return s_container.m_group_manager;
}

scheduler* singleton_manager::get_scheduler()
{
    return s_container.m_scheduler.load();
}

bool singleton_manager::set_scheduler(scheduler* ptr)
{
    scheduler* expected = nullptr;
    if (s_container.m_scheduler.compare_exchange_weak(expected, ptr))
    {
        ptr->start();
        s_container.m_network_manager = network_manager::create_singleton();
        s_container.m_network_manager->start();
        return true;
    }
    return false;
}

network_manager* singleton_manager::get_network_manager()
{
    network_manager* result = s_container.m_network_manager;
    if (result == nullptr)
    {
        scheduler* s = new thread_pool_scheduler;
        if (set_scheduler(s) == false)
        {
            delete s;
        }
        return get_network_manager();
    }
    return result;
}

empty_tuple* singleton_manager::get_empty_tuple()
{
    return s_container.m_empty_tuple;
}

} } // namespace cppa::detail
