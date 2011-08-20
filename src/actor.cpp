#include "cppa/config.hpp"

#include <map>
#include <mutex>
#include <atomic>
#include <stdexcept>

#include "cppa/actor.hpp"
#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"

namespace {

typedef std::lock_guard<cppa::util::shared_spinlock> exclusive_guard;
typedef cppa::util::shared_lock_guard<cppa::util::shared_spinlock> shared_guard;

} // namespace <anonmyous>

namespace cppa {
    
class actors_registry
{

public:

    actors_registry() : m_ids(1)
    {
    }
  
    void add(actor& act)
    {
        exclusive_guard guard(m_instances_mtx);
        m_instances.insert(std::make_pair(act.id(), &act));
    }
  
    void remove(actor& act)
    {
        exclusive_guard guard(m_instances_mtx);
        m_instances.erase(act.id());
    }
  
    intrusive_ptr<actor> find(std::uint32_t actor_id)
    {
        shared_guard guard(m_instances_mtx);
        auto i = m_instances.find(actor_id);
        if (i != m_instances.end())
        {
            return i->second;
        }
        return nullptr;
    }

    std::uint32_t next_id()
    {
        return m_ids.fetch_add(1);
    }
    
    static actors_registry* s_instance;
  
    static inline actors_registry& get()
    {
        return *s_instance;
    }

private:

    std::atomic<std::uint32_t> m_ids;
    std::map<std::uint32_t, cppa::actor*> m_instances;
    cppa::util::shared_spinlock m_instances_mtx;

};

actors_registry* actors_registry::s_instance = new actors_registry();

actor::actor(std::uint32_t aid, const process_information_ptr& pptr)
    : m_is_proxy(true), m_id(aid), m_parent_process(pptr)
{
    if (!pptr)
    {
        throw std::logic_error("parent == nullptr");
    }
}

actor::actor(const process_information_ptr& pptr)
    : m_is_proxy(false), m_id(actors_registry::get().next_id()), m_parent_process(pptr)
{
    if (!pptr)
    {
        throw std::logic_error("parent == nullptr");
    }
    else
    {
        actors_registry::get().add(*this);
    }
}

actor::~actor()
{
    if (!m_is_proxy)
    {
        actors_registry::get().remove(*this);
    }
}

intrusive_ptr<actor> actor::by_id(std::uint32_t actor_id)
{
    return actors_registry::get().find(actor_id);
}

void actor::join(group_ptr& what)
{
    if (!what) return;
    attach(what->subscribe(this));
}

void actor::leave(const group_ptr& what)
{
    if (!what) return;
    attachable::token group_token(typeid(group::unsubscriber), what.get());
    detach(group_token);
}

void actor::link_to(intrusive_ptr<actor>&& other)
{
    intrusive_ptr<actor> tmp(std::move(other));
    link_to(tmp);
}

void actor::unlink_from(intrusive_ptr<actor>&& other)
{
    intrusive_ptr<actor> tmp(std::move(other));
    unlink_from(tmp);
}

bool actor::remove_backlink(intrusive_ptr<actor>&& to)
{
    intrusive_ptr<actor> tmp(std::move(to));
    return remove_backlink(tmp);
}

bool actor::establish_backlink(intrusive_ptr<actor>&& to)
{
    intrusive_ptr<actor> tmp(std::move(to));
    return establish_backlink(tmp);
}

} // namespace cppa
