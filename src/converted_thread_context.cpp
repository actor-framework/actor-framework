#include "cppa/detail/converted_thread_context.hpp"

namespace cppa { namespace detail {

converted_thread_context::converted_thread_context() : m_exited(false)
{
}

void converted_thread_context::link(intrusive_ptr<actor>& other)
{
    std::lock_guard<std::mutex> guard(m_mtx);
    if (other && !m_exited && other->establish_backlink(this))
    {
        m_links.insert(other);
    }
}

bool converted_thread_context::remove_backlink(const intrusive_ptr<actor>& other)
{
    if (other && other != this)
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        return m_links.erase(other) > 0;
    }
    return false;
}

bool converted_thread_context::establish_backlink(const intrusive_ptr<actor>& other)
{
    bool send_exit_message = false;
    bool result = false;
    if (other && other != this)
    {
        // lifetime scope of guard
        {
            std::lock_guard<std::mutex> guard(m_mtx);
            if (!m_exited)
            {
                result = m_links.insert(other).second;
            }
            else
            {
                send_exit_message = true;
            }
        }
    }
    if (send_exit_message)
    {

    }
    return result;
}

void converted_thread_context::unlink(intrusive_ptr<actor>& other)
{
    std::lock_guard<std::mutex> guard(m_mtx);
    if (other && !m_exited && other->remove_backlink(this))
    {
        m_links.erase(other);
    }
}

void converted_thread_context::join(group_ptr& what)
{
    std::lock_guard<std::mutex> guard(m_mtx);
    if (!m_exited && m_subscriptions.count(what) == 0)
    {
        auto s = what->subscribe(this);
        // insert only valid subscriptions
        // (a subscription is invalid if this actor already joined the group)
        if (s) m_subscriptions.insert(std::make_pair(what, std::move(s)));
    }
}

void converted_thread_context::leave(const group_ptr& what)
{
    std::lock_guard<std::mutex> guard(m_mtx);
    m_subscriptions.erase(what);
}

message_queue& converted_thread_context::mailbox()
{
    return m_mailbox;
}

} } // namespace cppa::detail
