#ifndef ACTOR_PROXY_HPP
#define ACTOR_PROXY_HPP

#include "cppa/actor.hpp"

namespace cppa {

class actor_proxy : public actor
{

    process_information_ptr m_parent;

 public:

    actor_proxy(std::uint32_t mid, process_information_ptr&& parent);

    actor_proxy(std::uint32_t mid, const process_information_ptr& parent);

    bool attach(attachable* ptr);

    void detach(const attachable::token&);

    void enqueue(const message& msg);

    void join(group_ptr& what);

    void leave(const group_ptr& what);

    void link_to(intrusive_ptr<actor>& other);

    void unlink_from(intrusive_ptr<actor>& other);

    bool remove_backlink(const intrusive_ptr<actor>& to);

    bool establish_backlink(const intrusive_ptr<actor>& to);

    const process_information& parent_process() const;

    process_information_ptr parent_process_ptr() const;

};

typedef intrusive_ptr<actor_proxy> actor_proxy_ptr;

} // namespace cppa

#endif // ACTOR_PROXY_HPP
