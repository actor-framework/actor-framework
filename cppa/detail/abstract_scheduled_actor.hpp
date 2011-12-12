#ifndef SCHEDULED_ACTOR_HPP
#define SCHEDULED_ACTOR_HPP

#include "cppa/local_actor.hpp"
#include "cppa/scheduled_actor.hpp"
#include "cppa/abstract_actor.hpp"

#include "cppa/util/fiber.hpp"
#include "cppa/util/singly_linked_list.hpp"
#include "cppa/util/single_reader_queue.hpp"

#include "cppa/detail/delegate.hpp"

namespace cppa { namespace detail {

class task_scheduler;

/**
 * @brief A spawned, scheduled Actor.
 */
class abstract_scheduled_actor : public abstract_actor<local_actor>
{

    friend class task_scheduler;
    friend class util::single_reader_queue<abstract_scheduled_actor>;

    abstract_scheduled_actor* next; // intrusive next pointer (single_reader_queue)

    void enqueue_node(queue_node* node);

 protected:

    std::atomic<int> m_state;
    delegate m_enqueue_to_scheduler;

    typedef abstract_actor super;
    typedef super::queue_node queue_node;
    typedef util::singly_linked_list<queue_node> queue_node_buffer;

    enum dq_result
    {
        dq_done,
        dq_indeterminate,
        dq_timeout_occured
    };

    enum filter_result
    {
        normal_exit_signal,
        expired_timeout_message,
        timeout_message,
        ordinary_message
    };

    filter_result filter_msg(const any_tuple& msg);

    dq_result dq(std::unique_ptr<queue_node>& node,
                 invoke_rules_base& rules,
                 queue_node_buffer& buffer);

    bool has_pending_timeout()
    {
        return m_has_pending_timeout_request;
    }

    void request_timeout(const util::duration& d);

    void reset_timeout()
    {
        if (m_has_pending_timeout_request)
        {
            ++m_active_timeout_id;
            m_has_pending_timeout_request = false;
        }
    }

 private:

    bool m_has_pending_timeout_request;
    std::uint32_t m_active_timeout_id;
    pattern<atom_value, std::uint32_t> m_pattern;

 public:

    static constexpr int ready          = 0x00;
    static constexpr int done           = 0x01;
    static constexpr int blocked        = 0x02;
    static constexpr int about_to_block = 0x04;

    abstract_scheduled_actor();

    template<typename Scheduler>
    abstract_scheduled_actor(void (*enqueue_fun)(Scheduler*,
                                                 abstract_scheduled_actor*),
                    Scheduler* sched)
        : next(nullptr)
        , m_state(ready)
        , m_enqueue_to_scheduler(enqueue_fun, sched, this)
        , m_has_pending_timeout_request(false)
        , m_active_timeout_id(0)
    {
    }

    void quit(std::uint32_t reason);

    void enqueue(actor* sender, any_tuple&& msg);

    void enqueue(actor* sender, const any_tuple& msg);

    int compare_exchange_state(int expected, int new_value);

    struct resume_callback
    {
        virtual ~resume_callback();
        // actor could continue computation
        // - if false: actor interrupts
        // - if true: actor continues
        virtual bool still_ready() = 0;
        // called if an actor finished execution
        virtual void exec_done() = 0;
    };

    // from = calling worker
    virtual void resume(util::fiber* from, resume_callback* callback) = 0;

};

struct scheduled_actor_dummy : abstract_scheduled_actor
{
    void resume(util::fiber*, resume_callback*);
    void quit(std::uint32_t);
    void dequeue(invoke_rules&);
    void dequeue(timed_invoke_rules&);
    void link_to(intrusive_ptr<actor>&);
    void unlink_from(intrusive_ptr<actor>&);
    bool establish_backlink(intrusive_ptr<actor>&);
    bool remove_backlink(intrusive_ptr<actor>&);
    void detach(const attachable::token&);
    bool attach(attachable*);
};

} } // namespace cppa::detail

#endif // SCHEDULED_ACTOR_HPP
