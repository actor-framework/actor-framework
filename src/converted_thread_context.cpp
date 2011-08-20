#include <memory>
#include <algorithm>

#include "cppa/atom.hpp"
#include "cppa/exception.hpp"
#include "cppa/detail/converted_thread_context.hpp"

namespace cppa { namespace detail {

converted_thread_context::converted_thread_context(registry& registry)
    : abstract_actor<context>(registry)
{
}

void converted_thread_context::quit(std::uint32_t reason)
{
    try { super::cleanup(reason); } catch(...) { }
    // actor_exited should not be caught, but if anyone does,
    // the next call to current_context() must return a newly created instance
    parent_registry().set_current_context(nullptr);
    throw actor_exited(reason);
}

void converted_thread_context::cleanup(std::uint32_t reason)
{
    super::cleanup(reason);
}

message_queue& converted_thread_context::mailbox()
{
    return m_mailbox;
}

const message_queue& converted_thread_context::mailbox() const
{
    return m_mailbox;
}

} } // namespace cppa::detail
