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
            auto ptr = reinterpret_cast<const cppa::context*>(match_token.ptr);
            return m_client == ptr;
        }
        return false;
    }

};

} // namespace <anonymous>

namespace cppa {

context* operator<<(context* whom, const any_tuple& what)
{
    if (whom) whom->enqueue(message(self(), whom, what));
    return whom;
}

// matches self() << make_tuple(...)
context* operator<<(context* whom, any_tuple&& what)
{
    if (whom) whom->enqueue(message(self(), whom, std::move(what)));
    return whom;
}

void monitor(actor_ptr& whom)
{
    if (whom) whom->attach(new observer(self()));
}

void monitor(actor_ptr&& whom)
{
    monitor(static_cast<actor_ptr&>(whom));
}

void demonitor(actor_ptr& whom)
{
    attachable::token mtoken(typeid(observer), self());
    if (whom) whom->detach(mtoken);
}

void demonitor(actor_ptr&& whom)
{
    demonitor(static_cast<actor_ptr&>(whom));
}

} // namespace cppa
