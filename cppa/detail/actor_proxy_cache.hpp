#ifndef ACTOR_PROXY_CACHE_HPP
#define ACTOR_PROXY_CACHE_HPP

#include <string>
#include <functional>

#include "cppa/actor_proxy.hpp"
#include "cppa/process_information.hpp"

namespace cppa { namespace detail {

class actor_proxy_cache
{

 public:

    typedef std::tuple<std::uint32_t,                     // actor id
                       std::uint32_t,                     // process id
                       process_information::node_id_type> // node id
            key_tuple;

    typedef std::function<void (actor_proxy_ptr&)> new_proxy_callback;

 private:

    std::map<key_tuple, process_information_ptr> m_pinfos;
    std::map<key_tuple, actor_proxy_ptr> m_proxies;

    new_proxy_callback m_new_cb;

    process_information_ptr get_pinfo(const key_tuple& key);

 public:

    template<typename F>
    void set_callback(F&& cb)
    {
        m_new_cb = std::forward<F>(cb);
    }

    actor_proxy_ptr get(const key_tuple& key);

    void add(actor_proxy_ptr& pptr);

    size_t size() const;

    void erase(const actor_proxy_ptr& pptr);

    template<typename F>
    void for_each(F&& fun)
    {
        for (auto i = m_proxies.begin(); i != m_proxies.end(); ++i)
        {
            fun(i->second);
        }
    }

};

// get the thread-local cache object
actor_proxy_cache& get_actor_proxy_cache();

} } // namespace cppa::detail

#endif // ACTOR_PROXY_CACHE_HPP
