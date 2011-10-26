#include "cppa/cppa.hpp"

namespace {

class observer : public cppa::attachable
{

    cppa::actor_ptr m_client;

 public:

    observer(cppa::actor_ptr&& client) : m_client(std::move(client)) { }

    void detach(std::uint32_t reason)
    {
        cppa::send(m_client, cppa::atom(":Down"), reason);
    }

    bool matches(const cppa::attachable::token& match_token)
    {
        if (match_token.subtype == typeid(observer))
        {
            auto ptr = reinterpret_cast<const cppa::local_actor*>(match_token.ptr);
            return m_client == ptr;
        }
        return false;
    }

};

} // namespace <anonymous>

namespace cppa {

local_actor* operator<<(local_actor* whom, const any_tuple& what)
{
    if (whom) whom->enqueue(message(self(), whom, what));
    return whom;
}

// matches self() << make_tuple(...)
local_actor* operator<<(local_actor* whom, any_tuple&& what)
{
    if (whom) whom->enqueue(message(self(), whom, std::move(what)));
    return whom;
}

void link(actor_ptr& other)
{
    if (other) self()->link_to(other);
}

void link(actor_ptr&& other)
{
    actor_ptr tmp(std::move(other));
    link(tmp);
}

void link(actor_ptr& lhs, actor_ptr& rhs)
{
    if (lhs && rhs) lhs->link_to(rhs);
}

void link(actor_ptr&& lhs, actor_ptr& rhs)
{
    actor_ptr tmp(std::move(lhs));
    link(tmp, rhs);
}

void link(actor_ptr&& lhs, actor_ptr&& rhs)
{
    actor_ptr tmp1(std::move(lhs));
    actor_ptr tmp2(std::move(rhs));
    link(tmp1, tmp2);
}

void link(actor_ptr& lhs, actor_ptr&& rhs)
{
    actor_ptr tmp(std::move(rhs));
    link(lhs, tmp);
}

void unlink(actor_ptr& lhs, actor_ptr& rhs)
{
    if (lhs && rhs) lhs->unlink_from(rhs);
}

void monitor(actor_ptr& whom)
{
    if (whom) whom->attach(new observer(self()));
}

void monitor(actor_ptr&& whom)
{
    actor_ptr tmp(std::move(whom));
    monitor(tmp);
}

void demonitor(actor_ptr& whom)
{
    attachable::token mtoken(typeid(observer), self());
    if (whom) whom->detach(mtoken);
}

void receive_loop(invoke_rules& rules)
{
    auto& mbox = self()->mailbox();
    for (;;)
    {
        mbox.dequeue(rules);
    }
}

void receive_loop(timed_invoke_rules& rules)
{
    auto& mbox = self()->mailbox();
    for (;;)
    {
        mbox.dequeue(rules);
    }
}

} // namespace cppa
