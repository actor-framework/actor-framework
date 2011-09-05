#ifndef BLOCKING_MESSAGE_QUEUE_HPP
#define BLOCKING_MESSAGE_QUEUE_HPP

#include "cppa/message.hpp"
#include "cppa/message_queue.hpp"
#include "cppa/util/singly_linked_list.hpp"
#include "cppa/util/single_reader_queue.hpp"
#include "cppa/detail/abstract_message_queue.hpp"

namespace cppa { namespace detail {

// blocks if single_reader_queue blocks
class blocking_message_queue_impl : public message_queue
{

 public:

    struct queue_node
    {
        queue_node* next;
        message msg;
        queue_node(const message& from);
    };

    typedef util::single_reader_queue<queue_node> queue_type;

 private:

    queue_type m_queue;

 protected:

    typedef util::singly_linked_list<queue_node> queue_node_buffer;

    inline bool empty()
    {
        return m_queue.empty();
    }

    inline void restore_mailbox(queue_node_buffer& buffer)
    {
        if (!buffer.empty()) m_queue.push_front(std::move(buffer));
    }

    // conputes the next message, returns true if the computed message
    // was not ignored (e.g. because it's an exit message with reason = normal)
    bool dequeue_impl(message& storage);

    bool dequeue_impl(invoke_rules&, queue_node_buffer&);

 public:

    inline queue_type& queue()
    {
        return m_queue;
    }

    inline const queue_type& queue() const
    {
        return m_queue;
    }

    virtual void enqueue(const message& msg) /*override*/;
};

typedef abstract_message_queue<blocking_message_queue_impl>
        blocking_message_queue;

} } // namespace hamcast::detail

#endif // BLOCKING_MESSAGE_QUEUE_HPP
