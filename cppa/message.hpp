#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include "cppa/actor.hpp"
#include "cppa/tuple.hpp"
#include "cppa/channel.hpp"
#include "cppa/untyped_tuple.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/detail/channel.hpp"

namespace cppa {

class message
{

    struct content : ref_counted
    {
        const actor_ptr sender;
        const channel_ptr receiver;
        const untyped_tuple data;
        inline content(const actor_ptr& s,
                       const channel_ptr& r,
                       const untyped_tuple& ut)
            : ref_counted(), sender(s), receiver(r), data(ut)
        {
        }
    };

    intrusive_ptr<content> m_content;

 public:

    template<typename... Args>
    message(const actor_ptr& from, const channel_ptr& to, const Args&... args)
        : m_content(new content(from, to, tuple<Args...>(args...)))
    {
    }

    message(const actor_ptr& from,
            const channel_ptr& to,
            const untyped_tuple& ut);

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

    inline const untyped_tuple& data() const
    {
        return m_content->data;
    }

};

bool operator==(const message& lhs, const message& rhs);

inline bool operator!=(const message& lhs, const message& rhs)
{
    return !(lhs == rhs);
}

} // namespace cppa

#endif // MESSAGE_HPP
