#ifndef CONVERTED_THREAD_CONTEXT_HPP
#define CONVERTED_THREAD_CONTEXT_HPP

#include "cppa/config.hpp"

#include <map>
#include <list>
#include <mutex>
#include <atomic>
#include <vector>
#include <memory>
#include <cstdint>

#include "cppa/context.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/detail/abstract_actor.hpp"
#include "cppa/detail/blocking_message_queue.hpp"

namespace cppa { namespace detail {

class converted_thread_context : public abstract_actor<context>
{

    typedef abstract_actor<context> super;

 public:

    // mailbox implementation
    blocking_message_queue m_mailbox;

    message_queue& mailbox() /*override*/;

    const message_queue& mailbox() const /*override*/;

    // called if the converted thread finished execution
    void cleanup(std::uint32_t reason = exit_reason::normal);

    void quit(std::uint32_t reason) /*override*/;

};

} } // namespace cppa::detail

#endif // CONVERTED_THREAD_CONTEXT_HPP
