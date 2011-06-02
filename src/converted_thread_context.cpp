#include <algorithm>

#include "cppa/exit_signal.hpp"
#include "cppa/actor_exited.hpp"
#include "cppa/detail/converted_thread_context.hpp"

namespace {

template<class List, typename Element>
bool unique_insert(List& lst, const Element& e)
{
    auto end = lst.end();
    auto i = std::find(lst.begin(), end, e);
    if (i == end)
    {
        lst.push_back(e);
        return true;
    }
    return false;
}

template<class List, typename Iterator, typename Element>
int erase_all(List& lst, Iterator from, Iterator end, const Element& e)
{
    auto i = std::find(from, end, e);
    if (i != end)
    {
        return 1 + erase_all(lst, lst.erase(i), end, e);
    }
    return 0;
}

template<class List, typename Element>
int erase_all(List& lst, const Element& e)
{
    return erase_all(lst, lst.begin(), lst.end(), e);
}

} // namespace <anonymous>

namespace cppa { namespace detail {

converted_thread_context::converted_thread_context() : m_exited(false)
{
}

void converted_thread_context::quit(std::uint32_t reason)
{
    decltype(m_links) mlinks;
    decltype(m_subscriptions) msubscriptions;
    // lifetime scope of guard
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        m_exited = true;
        mlinks = std::move(m_links);
        msubscriptions = std::move(m_subscriptions);
        // make sure m_links and m_subscriptions definitely are empty
        m_links.clear();
        m_subscriptions.clear();
    }
    actor_ptr mself = self();
    // send exit messages
    for (actor_ptr& aptr : mlinks)
    {

        aptr->enqueue(message(mself, aptr, exit_signal(reason)));
    }
    throw actor_exited(reason);
}

void converted_thread_context::link(intrusive_ptr<actor>& other)
{
    std::lock_guard<std::mutex> guard(m_mtx);
    if (other && !m_exited && other->establish_backlink(this))
    {
        m_links.push_back(other);
        //m_links.insert(other);
    }
}

bool converted_thread_context::remove_backlink(const intrusive_ptr<actor>& other)
{
    if (other && other != this)
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        return erase_all(m_links, other) > 0;//m_links.erase(other) > 0;
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
                result = unique_insert(m_links, other);
                //result = m_links.insert(other).second;
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
        erase_all(m_links, other);
        //m_links.erase(other);
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
