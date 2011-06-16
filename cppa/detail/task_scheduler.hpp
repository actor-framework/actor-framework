#ifndef TASK_SCHEDULER_HPP
#define TASK_SCHEDULER_HPP

#include "cppa/scheduler.hpp"

namespace cppa { namespace detail {

class task_scheduler : public scheduler
{

 public:

    void await_others_done();
    void register_converted_context(context*);
    //void unregister_converted_context(context*);
    actor_ptr spawn(actor_behavior*, scheduling_hint);
    attachable* register_hidden_context();

};

} } // namespace cppa::detail

#endif // TASK_SCHEDULER_HPP
