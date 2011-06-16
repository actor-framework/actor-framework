#include "cppa/detail/task_scheduler.hpp"

namespace cppa { namespace detail {

void task_scheduler::await_others_done()
{
}

void task_scheduler::register_converted_context(context*)
{
}

actor_ptr task_scheduler::spawn(actor_behavior*, scheduling_hint)
{
    return nullptr;
}

attachable* task_scheduler::register_hidden_context()
{
    return nullptr;
}

} } // namespace cppa::detail
