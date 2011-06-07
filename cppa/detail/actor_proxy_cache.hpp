#ifndef ACTOR_PROXY_CACHE_HPP
#define ACTOR_PROXY_CACHE_HPP

#include <string>

#include "cppa/actor_proxy.hpp"
#include "cppa/process_information.hpp"

namespace cppa { namespace detail {

class actor_proxy_cache
{

 public:

    typedef std::tuple<std::uint32_t, std::uint32_t,
                       process_information::node_id_type> key_tuple;

 private:

    std::map<key_tuple, process_information_ptr> m_pinfos;
    std::map<key_tuple, actor_proxy_ptr> m_proxies;

    process_information_ptr get_pinfo(const key_tuple& key);
    void add(const actor_proxy_ptr& pptr, const key_tuple& key);

 public:

    actor_proxy_ptr get(const key_tuple& key);
    void add(const actor_proxy_ptr& pptr);

};

// get the thread-local cache object
actor_proxy_cache& get_actor_proxy_cache();

} } // namespace cppa::detail

#endif // ACTOR_PROXY_CACHE_HPP
