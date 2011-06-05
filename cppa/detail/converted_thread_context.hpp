#ifndef CONVERTED_THREAD_CONTEXT_HPP
#define CONVERTED_THREAD_CONTEXT_HPP

#include "cppa/config.hpp"

#include <map>
#include <list>
#include <mutex>
#include <vector>
#include <memory>

#include "cppa/context.hpp"
#include "cppa/detail/blocking_message_queue.hpp"

namespace cppa { namespace detail {

class converted_thread_context : public context
{

    // true if the associated thread has finished execution
    std::uint32_t m_exit_reason;

    // mailbox implementation
    detail::blocking_message_queue m_mailbox;

    // guards access to m_exited, m_subscriptions and m_links
    std::mutex m_mtx;

    // manages group subscriptions
    std::map<group_ptr, group::subscription> m_subscriptions;

    // manages actor links
    std::list<actor_ptr> m_links;

    std::vector<std::unique_ptr<attachable>> m_attachables;

    // @pre m_mtx is locked
    inline bool exited() const
    {
        return m_exit_reason != exit_reason::not_exited;
    }

 public:

    converted_thread_context();

    message_queue& mailbox /*[[override]]*/ ();

    // called if the converted thread finished execution
    void cleanup(std::uint32_t reason = exit_reason::normal);

    bool attach /*[[override]]*/ (attachable* ptr);

    void join /*[[override]]*/ (group_ptr& what);

    void quit /*[[override]]*/ (std::uint32_t reason);

    void leave /*[[override]]*/ (const group_ptr& what);

    void link_to /*[[override]]*/ (intrusive_ptr<actor>& other);

    void unlink_from /*[[override]]*/ (intrusive_ptr<actor>& other);

    bool remove_backlink /*[[override]]*/ (const intrusive_ptr<actor>& to);

    bool establish_backlink /*[[override]]*/ (const intrusive_ptr<actor>& to);

};

} } // namespace cppa::detail

#endif // CONVERTED_THREAD_CONTEXT_HPP
