#ifndef BLOCKING_MESSAGE_QUEUE_HPP
#define BLOCKING_MESSAGE_QUEUE_HPP

#include "cppa/any_tuple.hpp"
#include "cppa/message_queue.hpp"
#include "cppa/util/singly_linked_list.hpp"
#include "cppa/util/single_reader_queue.hpp"
#include "cppa/detail/abstract_message_queue.hpp"

namespace cppa { class invoke_rules_base; }

namespace cppa { namespace detail {

// blocks if single_reader_queue blocks
class blocking_message_queue_impl : public message_queue
{

 public:

    struct queue_node
    {
        queue_node* next;
        any_tuple msg;
        queue_node(const any_tuple& from);
    };

    typedef util::single_reader_queue<queue_node> queue_type;

    inline queue_type& queue()
    {
        return m_queue;
    }

    inline const queue_type& queue() const
    {
        return m_queue;
    }

    virtual void enqueue(const any_tuple& msg) /*override*/;

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

    // computes the next message, returns true if the computed message
    // was not ignored (e.g. because it's an exit message with reason = normal)
    bool dequeue_impl(any_tuple& storage);

    bool dequeue_impl(invoke_rules&, queue_node_buffer&);

    bool dequeue_impl(timed_invoke_rules&, queue_node_buffer&);

 private:


    bool dq(std::unique_ptr<queue_node>&,invoke_rules_base&,queue_node_buffer&);

   queue_type m_queue;

};

typedef abstract_message_queue<blocking_message_queue_impl>
        blocking_message_queue;

} } // namespace hamcast::detail

#endif // BLOCKING_MESSAGE_QUEUE_HPP
