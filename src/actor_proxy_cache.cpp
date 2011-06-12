#include <boost/thread.hpp>

#include "cppa/atom.hpp"
#include "cppa/message.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"

namespace {

boost::thread_specific_ptr<cppa::detail::actor_proxy_cache> t_proxy_cache;

} // namespace <anonymous>

namespace cppa { namespace detail {

process_information_ptr
actor_proxy_cache::get_pinfo(const actor_proxy_cache::key_tuple& key)
{
    auto i = m_pinfos.find(key);
    if (i != m_pinfos.end())
    {
        return i->second;
    }
    process_information_ptr tmp(new process_information(std::get<1>(key),
                                                        std::get<2>(key)));
    m_pinfos.insert(std::make_pair(key, tmp));
    return tmp;
}

actor_proxy_ptr actor_proxy_cache::get(const key_tuple& key)
{
    auto i = m_proxies.find(key);
    if (i != m_proxies.end())
    {
        return i->second;
    }
    // get_pinfo(key) also inserts to m_pinfos
    actor_proxy_ptr result(new actor_proxy(std::get<0>(key), get_pinfo(key)));
    // insert to m_proxies
    m_proxies.insert(std::make_pair(key, result));
    result->enqueue(message(result, nullptr, atom(":Monitor")));
    return result;
}

void actor_proxy_cache::add(const actor_proxy_ptr& pptr, const key_tuple& key)
{
    m_pinfos.insert(std::make_pair(key, pptr->parent_process_ptr()));
    m_proxies.insert(std::make_pair(key, pptr));
}

void actor_proxy_cache::add(const actor_proxy_ptr& pptr)
{
    auto pinfo = pptr->parent_process_ptr();
    key_tuple key(pptr->id(), pinfo->process_id, pinfo->node_id);
    add(pptr, key);
}

size_t actor_proxy_cache::size() const
{
    return m_proxies.size();
}

actor_proxy_cache& get_actor_proxy_cache()
{
    if (t_proxy_cache.get() == nullptr)
    {
        t_proxy_cache.reset(new actor_proxy_cache);
    }
    return *t_proxy_cache.get();
}

} } // namespace cppa::detail
