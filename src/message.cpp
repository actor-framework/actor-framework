#include "cppa/message.hpp"

namespace cppa {

message::msg_content* message::create_dummy()
{
    msg_content* result = new msg_content(0, 0, tuple<int>(0));
    result->ref();
    return result;
}

message::msg_content* message::s_dummy = message::create_dummy();

message::message(const actor_ptr& from,
                 const channel_ptr& to,
                 const any_tuple& ut)
    : m_content(new msg_content(from, to, ut))
{
}

message::message(const actor_ptr& from,
                 const channel_ptr& to,
                 any_tuple&& ut)
    : m_content(new msg_content(from, to, std::move(ut)))
{
}

message::message() : m_content(s_dummy)
{
}

bool message::empty() const
{
    return m_content.get() == s_dummy;
}

bool operator==(const message& lhs, const message& rhs)
{
    return    lhs.sender() == rhs.sender()
           && lhs.receiver() == rhs.receiver()
           && lhs.content().vals()->equal_to(*(rhs.content().vals()));
}

} // namespace cppa
