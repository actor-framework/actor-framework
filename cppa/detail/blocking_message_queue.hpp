#ifndef BLOCKING_MESSAGE_QUEUE_HPP
#define BLOCKING_MESSAGE_QUEUE_HPP

#include "cppa/message.hpp"
#include "cppa/message_queue.hpp"
#include "cppa/util/single_reader_queue.hpp"

namespace cppa { namespace detail {

// blocks if single_reader_queue blocks
class blocking_message_queue : public message_queue
{

    struct queue_node
    {
        queue_node* next;
        message msg;
        queue_node(const message& from) : next(0), msg(from) { }
    };

    bool m_trap_exit;
    message m_last_dequeued;
    util::single_reader_queue<queue_node> m_queue;

 public:

    blocking_message_queue();

    virtual void trap_exit(bool new_value);

    virtual void enqueue(const message& msg) /*override*/;

    virtual const message& dequeue() /*override*/;

    virtual void dequeue(invoke_rules& rules) /*override*/;

    virtual bool try_dequeue(message& msg) /*override*/;

    virtual bool try_dequeue(invoke_rules& rules) /*override*/;

    virtual const message& last_dequeued() /*override*/;

};

} } // namespace hamcast::detail

#endif // BLOCKING_MESSAGE_QUEUE_HPP
