// for thread_specific_ptr
// needed unless the new keyword "thread_local" works in GCC
#include <boost/thread.hpp>

#include "cppa/context.hpp"
#include "cppa/message.hpp"
#include "cppa/scheduler.hpp"

#include "cppa/detail/converted_thread_context.hpp"

using cppa::detail::converted_thread_context;

namespace {

void cleanup_fun(cppa::context* what)
{
    if (what && !what->deref()) delete what;
}

boost::thread_specific_ptr<cppa::context> t_this_context(cleanup_fun);

} // namespace <anonymous>

namespace cppa {

void context::enqueue(const message& msg)
{
    mailbox().enqueue(msg);
}

void context::trap_exit(bool new_value)
{
    mailbox().trap_exit(new_value);
}

context* unchecked_self()
{
    return t_this_context.get();
}

context* self()
{
    context* result = t_this_context.get();
    if (result == nullptr)
    {
        result = new converted_thread_context;
        result->ref();
        get_scheduler()->register_converted_context(result);
        t_this_context.reset(result);
    }
    return result;
}

void set_self(context* ctx)
{
    if (ctx) ctx->ref();
    t_this_context.reset(ctx);
}

} // namespace cppa
