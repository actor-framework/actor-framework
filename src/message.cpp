#include "cppa/message.hpp"

namespace cppa {

message::message(const actor_ptr& from,
                 const channel_ptr& to,
                 const untyped_tuple& ut)
    : m_content(new content(from, to, ut))
{
}

message::message() : m_content(new content(0, 0, tuple<int>(0)))
{
}

bool operator==(const message& lhs, const message& rhs)
{
    return    lhs.sender() == rhs.sender()
           && lhs.receiver() == rhs.receiver()
           && lhs.data().vals()->equal_to(*(rhs.data().vals()));
}

} // namespace cppa
