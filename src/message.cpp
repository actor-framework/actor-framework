#include "cppa/message.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa {

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

message::message() : m_content(detail::singleton_manager::get_message_dummy())
{
}

bool message::empty() const
{
    return m_content.get() == detail::singleton_manager::get_message_dummy();
}

bool operator==(const message& lhs, const message& rhs)
{
    return    lhs.sender() == rhs.sender()
           && lhs.receiver() == rhs.receiver()
           && lhs.content().vals()->equal_to(*(rhs.content().vals()));
}

} // namespace cppa
