#include <boost/thread.hpp>

#include "cppa/context.hpp"
#include "cppa/message.hpp"
#include "cppa/scheduler.hpp"

#include "cppa/detail/converted_thread_context.hpp"

namespace {

void cleanup_fun(cppa::context* what)
{
    if (what && !what->deref())
    {
        delete what;
    }
}

boost::thread_specific_ptr<cppa::context> m_this_context(cleanup_fun);

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
    return m_this_context.get();
}

context* self()
{
    context* result = m_this_context.get();
    if (!result)
    {
        result = new detail::converted_thread_context;
        result->ref();
        get_scheduler()->register_converted_context(result);
        m_this_context.reset(result);
    }
    return result;
}

void set_self(context* ctx)
{
    if (ctx) ctx->ref();
    context* old = m_this_context.get();
    m_this_context.reset(ctx);
    if (old)
    {
        cleanup_fun(ctx);
    }
}

} // namespace cppa
