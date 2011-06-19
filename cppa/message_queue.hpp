#ifndef MESSAGE_QUEUE_HPP
#define MESSAGE_QUEUE_HPP

#include "cppa/message.hpp"
#include "cppa/ref_counted.hpp"

namespace cppa {

// forward declaration
class invoke_rules;

class message_queue : public ref_counted
{

 protected:

    bool m_trap_exit;
    message m_last_dequeued;

 public:

    message_queue();

    virtual void enqueue(const message&) = 0;
    virtual const message& dequeue() = 0;
    virtual void dequeue(invoke_rules&) = 0;
    virtual bool try_dequeue(message&) = 0;
    virtual bool try_dequeue(invoke_rules&) = 0;

    inline void trap_exit(bool value);
    inline const message& last_dequeued();

};

inline void message_queue::trap_exit(bool value)
{
    m_trap_exit = value;
}

inline const message& message_queue::last_dequeued()
{
    return m_last_dequeued;
}

} // namespace cppa

#endif // MESSAGE_QUEUE_HPP
