#ifndef SCHEDULED_ACTOR_HPP
#define SCHEDULED_ACTOR_HPP

#include "cppa/local_actor.hpp"
#include "cppa/util/fiber.hpp"
#include "cppa/actor_behavior.hpp"
#include "cppa/util/single_reader_queue.hpp"
#include "cppa/detail/delegate.hpp"
#include "cppa/detail/abstract_actor.hpp"

namespace cppa { namespace detail {

class task_scheduler;

/**
 * @brief A spawned, scheduled Actor.
 */
class scheduled_actor : public abstract_actor<local_actor>
{

    friend class task_scheduler;
    friend class util::single_reader_queue<scheduled_actor>;

    scheduled_actor* next;    // intrusive next pointer (single_reader_queue)

    void enqueue_node(queue_node* node);

 protected:

    std::atomic<int> m_state;
    delegate m_enqueue_to_scheduler;

    typedef abstract_actor<local_actor> super;
    typedef super::queue_node queue_node;

 public:

    static constexpr int ready          = 0x00;
    static constexpr int done           = 0x01;
    static constexpr int blocked        = 0x02;
    static constexpr int about_to_block = 0x04;

    scheduled_actor();

    template<typename Scheduler>
    scheduled_actor(void (*enqueue_fun)(Scheduler*, scheduled_actor*),
                    Scheduler* sched)
        : next(nullptr)
        , m_state(ready)
        , m_enqueue_to_scheduler(enqueue_fun, sched, this)
    {
    }

    void quit(std::uint32_t reason);

    void enqueue(actor* sender, any_tuple&& msg);

    void enqueue(actor* sender, const any_tuple& msg);

    //inline std::atomic<int>& state() { return m_mailbox.m_state; }

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

struct scheduled_actor_dummy : scheduled_actor
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
