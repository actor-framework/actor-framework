#ifndef ACTOR_PROXY_HPP
#define ACTOR_PROXY_HPP

#include "cppa/actor.hpp"
#include "cppa/detail/abstract_actor.hpp"

namespace cppa {

class actor_proxy : public detail::abstract_actor<actor>
{

    typedef detail::abstract_actor<actor> super;

    // implemented in unicast_network.cpp
    static void forward_message(const process_information_ptr&, const message&);

 public:

    actor_proxy(std::uint32_t mid, const process_information_ptr& parent);

    void enqueue(const message& msg);

    void link_to(intrusive_ptr<actor>& other);

    void unlink_from(intrusive_ptr<actor>& other);

    bool remove_backlink(intrusive_ptr<actor>& to);

    bool establish_backlink(intrusive_ptr<actor>& to);

};

typedef intrusive_ptr<actor_proxy> actor_proxy_ptr;

} // namespace cppa

#endif // ACTOR_PROXY_HPP
