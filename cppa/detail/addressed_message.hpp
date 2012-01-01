#ifndef ADDRESSED_MESSAGE_HPP
#define ADDRESSED_MESSAGE_HPP

#include "cppa/actor.hpp"
#include "cppa/tuple.hpp"
#include "cppa/channel.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/tuple_view.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/detail/channel.hpp"

namespace cppa { namespace detail {

class addressed_message
{

 public:

    addressed_message(actor_ptr const& from,
            channel_ptr const& to,
            any_tuple const& ut);

    addressed_message(actor_ptr const& from,
            channel_ptr const& to,
            any_tuple&& ut);

    addressed_message() = default;
    addressed_message(addressed_message&&) = default;
    addressed_message(addressed_message const&) = default;
    addressed_message& operator=(addressed_message&&) = default;
    addressed_message& operator=(addressed_message const&) = default;

    inline actor_ptr& sender()
    {
        return m_sender;
    }

    inline actor_ptr const& sender() const
    {
        return m_sender;
    }

    inline channel_ptr& receiver()
    {
        return m_receiver;
    }

    inline channel_ptr const& receiver() const
    {
        return m_receiver;
    }

    inline any_tuple& content()
    {
        return m_content;
    }

    inline any_tuple const& content() const
    {
        return m_content;
    }

    inline bool empty() const
    {
        return m_content.empty();
    }

 private:

    actor_ptr m_sender;
    channel_ptr m_receiver;
    any_tuple m_content;

};

bool operator==(addressed_message const& lhs, addressed_message const& rhs);

inline bool operator!=(addressed_message const& lhs, addressed_message const& rhs)
{
    return !(lhs == rhs);
}

} } // namespace cppa::detail

#endif // ADDRESSED_MESSAGE_HPP
