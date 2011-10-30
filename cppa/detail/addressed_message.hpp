#ifndef ADDRESSED_MESSAGE_HPP
#define ADDRESSED_MESSAGE_HPP

#include <map>
#include <vector>
#include "cppa/actor.hpp"
#include "cppa/channel.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/process_information.hpp"

namespace cppa { class serializer; class deserializer; }

namespace cppa { namespace detail {

class addressed_message
{

    friend struct addr_msg_tinfo;

 public:

    addressed_message() = default;

    addressed_message(addressed_message&&) = default;

    addressed_message(const addressed_message&) = default;

    addressed_message& operator=(addressed_message&&) = default;

    addressed_message& operator=(const addressed_message&) = default;

    struct process_ptr_less
    {
        bool operator()(const process_information_ptr& lhs,
                        const process_information_ptr& rhs);
    };

    typedef std::map<process_information_ptr,
                     std::vector<actor_id>,
                     process_ptr_less> receiver_map;

    inline const receiver_map& receivers() const;

    inline receiver_map& receivers();

    inline const any_tuple& content() const;

    inline any_tuple& content();

 private:

    void serialize_to(serializer* sink) const;

    void deserialize_from(deserializer* source);

    any_tuple m_content;
    receiver_map m_receivers;

};

bool operator==(const addressed_message& lhs, const addressed_message& rhs);

inline bool operator!=(const addressed_message& lhs,
                       const addressed_message& rhs)
{
    return !(lhs == rhs);
}

inline const any_tuple& addressed_message::content() const
{
    return m_content;
}

inline any_tuple& addressed_message::content()
{
    return m_content;
}

inline const addressed_message::receiver_map&
addressed_message::receivers() const
{
    return m_receivers;
}

inline addressed_message::receiver_map& addressed_message::receivers()
{
    return m_receivers;
}

} } // namespace cppa::detail

#endif // ADDRESSED_MESSAGE_HPP
