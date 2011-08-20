#include "cppa/context.hpp"
#include "cppa/message.hpp"
#include "cppa/scheduler.hpp"

#include "cppa/detail/converted_thread_context.hpp"

using cppa::detail::converted_thread_context;

namespace cppa {

context::context(registry& registry) : actor(registry)
{
}

void context::enqueue(const message& msg)
{
    mailbox().enqueue(msg);
}

} // namespace cppa
