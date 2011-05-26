#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include "cppa/actor.hpp"
#include "cppa/tuple.hpp"
#include "cppa/channel.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/tuple_view.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/detail/channel.hpp"

namespace cppa {

class message
{

    struct msg_content : ref_counted
    {
        const actor_ptr sender;
        const channel_ptr receiver;
        const any_tuple data;
        inline msg_content(const actor_ptr& s,
                           const channel_ptr& r,
                           const any_tuple& ut)
            : ref_counted(), sender(s), receiver(r), data(ut)
        {
        }
        inline msg_content(const actor_ptr& s,
                           const channel_ptr& r,
                           any_tuple&& ut)
            : ref_counted(), sender(s), receiver(r), data(std::move(ut))
        {
        }
    };

    intrusive_ptr<msg_content> m_content;

 public:

    template<typename... Args>
    message(const actor_ptr& from, const channel_ptr& to, const Args&... args);

    message(const actor_ptr& from,
            const channel_ptr& to,
            const any_tuple& ut);

    message(const actor_ptr& from,
            const channel_ptr& to,
            any_tuple&& ut);

    message();

    message& operator=(const message&) = default;

    message& operator=(message&&) = default;

    message(const message&) = default;

    message(message&&) = default;

    inline const actor_ptr& sender() const
    {
        return m_content->sender;
    }

    inline const channel_ptr& receiver() const
    {
        return m_content->receiver;
    }

    inline const any_tuple& content() const
    {
        return m_content->data;
    }

};

template<typename... Args>
message::message(const actor_ptr& from, const channel_ptr& to, const Args&... args)
    : m_content(new msg_content(from, to, tuple<Args...>(args...)))
{
}

bool operator==(const message& lhs, const message& rhs);

inline bool operator!=(const message& lhs, const message& rhs)
{
    return !(lhs == rhs);
}

} // namespace cppa

#endif // MESSAGE_HPP
