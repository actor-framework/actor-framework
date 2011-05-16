#include "cppa/message.hpp"

namespace cppa {

message::message(const actor_ptr& from,
                 const channel_ptr& to,
                 const any_tuple& ut)
    : m_content(new msg_content(from, to, ut))
{
}

message::message() : m_content(new msg_content(0, 0, tuple<int>(0)))
{
}

bool operator==(const message& lhs, const message& rhs)
{
    return    lhs.sender() == rhs.sender()
           && lhs.receiver() == rhs.receiver()
           && lhs.content().vals()->equal_to(*(rhs.content().vals()));
}

} // namespace cppa
