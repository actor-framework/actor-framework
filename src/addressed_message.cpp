#include "cppa/detail/addressed_message.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa { namespace detail {

addressed_message::addressed_message(const actor_ptr& from,
                 const channel_ptr& to,
                 const any_tuple& ut)
    : m_sender(from), m_receiver(to), m_content(ut)
{
}

addressed_message::addressed_message(const actor_ptr& from,
                 const channel_ptr& to,
                 any_tuple&& ut)
    : m_sender(from), m_receiver(to), m_content(std::move(ut))
{
}

bool operator==(const addressed_message& lhs, const addressed_message& rhs)
{
    return    lhs.sender() == rhs.sender()
           && lhs.receiver() == rhs.receiver()
           && lhs.content() == rhs.content();
}

} } // namespace cppa
