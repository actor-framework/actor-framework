#include <memory>
#include <algorithm>

#include "cppa/atom.hpp"
#include "cppa/exception.hpp"
#include "cppa/detail/converted_thread_context.hpp"

namespace cppa { namespace detail {

void converted_thread_context::quit(std::uint32_t reason)
{
    try { super::cleanup(reason); } catch(...) { }
    // actor_exited should not be catched, but if anyone does,
    // the next call to self() must return a newly created instance
    set_self(nullptr);
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

} } // namespace cppa::detail
