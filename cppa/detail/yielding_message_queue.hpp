#ifndef YIELDING_MESSAGE_QUEUE_HPP
#define YIELDING_MESSAGE_QUEUE_HPP

#include <cstdint>

#include "cppa/any_tuple.hpp"
#include "cppa/message_queue.hpp"
#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"
#include "cppa/util/singly_linked_list.hpp"
#include "cppa/util/single_reader_queue.hpp"
#include "cppa/detail/abstract_message_queue.hpp"

namespace cppa { class invoke_rules_base; }

namespace cppa { namespace detail {

class scheduled_actor;

enum actor_state
{
    ready,
    done,
    blocked,
    about_to_block
};

// blocks if single_reader_queue blocks
class yielding_message_queue_impl : public message_queue
{

    friend class scheduled_actor;

    struct queue_node
    {
        queue_node* next;
        actor_ptr sender;
        any_tuple msg;
        queue_node(actor* from, any_tuple&& content);
        queue_node(actor* from, const any_tuple& content);
    };

    bool m_has_pending_timeout_request;
    std::uint32_t m_active_timeout_id;
    scheduled_actor* m_parent;
    std::atomic<int> m_state;
    const uniform_type_info* m_atom_value_uti;
    const uniform_type_info* m_ui32_uti;
    const uniform_type_info* m_actor_ptr_uti;
    util::single_reader_queue<queue_node> m_queue;

    void yield_until_not_empty();

    void enqueue_node(queue_node*);

    enum dq_result
    {
        done,
        indeterminate,
        timeout_occured
    };

    enum filter_result
    {
        normal_exit_signal,
        expired_timeout_message,
        timeout_message,
        ordinary_message
    };

    filter_result filter_msg(const any_tuple& msg);

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

    dq_result dq(std::unique_ptr<queue_node>&,
                 invoke_rules_base&,
                 queue_node_buffer&);

    // conputes the next message, returns true if the computed message
    // was not ignored (e.g. because it's an exit message with reason = normal)
    bool dequeue_impl(any_tuple& storage);

    bool dequeue_impl(invoke_rules&, queue_node_buffer&);

    bool dequeue_impl(timed_invoke_rules&, queue_node_buffer&);

 public:

    yielding_message_queue_impl(scheduled_actor* parent);

    ~yielding_message_queue_impl();

    void enqueue(actor* sender, any_tuple&& msg) /*override*/;

    void enqueue(actor* sender, const any_tuple& msg) /*override*/;

};

typedef abstract_message_queue<yielding_message_queue_impl>
        yielding_message_queue;

} } // namespace hamcast::detail

#endif // YIELDING_MESSAGE_QUEUE_HPP
