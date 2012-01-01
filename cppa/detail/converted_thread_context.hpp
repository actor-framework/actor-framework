#ifndef CONVERTED_THREAD_CONTEXT_HPP
#define CONVERTED_THREAD_CONTEXT_HPP

#include "cppa/config.hpp"

#include <map>
#include <list>
#include <mutex>
#include <stack>
#include <atomic>
#include <vector>
#include <memory>
#include <cstdint>

#include "cppa/atom.hpp"
#include "cppa/pattern.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/abstract_actor.hpp"

#include "cppa/util/either.hpp"
#include "cppa/util/singly_linked_list.hpp"

namespace cppa { namespace detail {

/**
 * @brief Represents a thread that was converted to an Actor.
 */
class converted_thread_context : public abstract_actor<local_actor>
{

    typedef abstract_actor<local_actor> super;
    typedef super::queue_node queue_node;

 public:

    converted_thread_context();

    // called if the converted thread finished execution
    void cleanup(std::uint32_t reason = exit_reason::normal);

    void quit(std::uint32_t reason) /*override*/;

    void enqueue(actor* sender, any_tuple&& msg) /*override*/;

    void enqueue(actor* sender, any_tuple const& msg) /*override*/;

    void dequeue(invoke_rules& rules) /*override*/;

    void dequeue(timed_invoke_rules& rules)  /*override*/;

    inline util::single_reader_queue<queue_node>& mailbox()
    {
        return m_mailbox;
    }

 private:

    typedef util::singly_linked_list<queue_node> queue_node_buffer;

    enum throw_on_exit_result
    {
        not_an_exit_signal,
        normal_exit_signal
    };

    // returns true if node->msg was accepted by rules
    bool dq(std::unique_ptr<queue_node>& node,
            invoke_rules_base& rules,
            queue_node_buffer& buffer);

    throw_on_exit_result throw_on_exit(any_tuple const& msg);

    pattern<atom_value, actor_ptr, std::uint32_t> m_exit_msg_pattern;

};

} } // namespace cppa::detail

#endif // CONVERTED_THREAD_CONTEXT_HPP
