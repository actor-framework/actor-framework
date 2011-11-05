#ifndef ABSTRACT_ACTOR_HPP
#define ABSTRACT_ACTOR_HPP

#include "cppa/config.hpp"

#include <list>
#include <mutex>
#include <atomic>
#include <vector>
#include <memory>
#include <algorithm>

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/attachable.hpp"
#include "cppa/exit_reason.hpp"

namespace cppa { namespace detail {

/**
 * @brief Implements.
 */
template<class Base>
class abstract_actor : public Base
{

    typedef std::lock_guard<std::mutex> guard_type;
    typedef std::unique_ptr<attachable> attachable_ptr;

    // true if the associated thread has finished execution
    std::atomic<std::uint32_t> m_exit_reason;

    // guards access to m_exited, m_subscriptions and m_links
    std::mutex m_mtx;

    // manages actor links
    std::list<actor_ptr> m_links;

    std::vector<attachable_ptr> m_attachables;

    // @pre m_mtx.locked()
    bool exited() const
    {
        return m_exit_reason.load() != exit_reason::not_exited;
    }

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
    int erase_all(List& lst, Iterator begin, Iterator end, const Element& e)
    {
        auto i = std::find(begin, end, e);
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

 protected:

    //abstract_actor() : Base() { }

    template<typename... Args>
    abstract_actor(Args&&... args) : Base(std::forward<Args>(args)...)
                                   , m_exit_reason(exit_reason::not_exited)
    {
    }

    void cleanup(std::uint32_t reason)
    {
        if (reason == exit_reason::not_exited) return;
        decltype(m_links) mlinks;
        decltype(m_attachables) mattachables;
        // lifetime scope of guard
        {
            std::lock_guard<std::mutex> guard(m_mtx);
            if (m_exit_reason != exit_reason::not_exited)
            {
                // already exited
                return;
            }
            m_exit_reason = reason;
            mlinks = std::move(m_links);
            mattachables = std::move(m_attachables);
            // make sure lists are definitely empty
            m_links.clear();
            m_attachables.clear();
        }
        if (!mlinks.empty())
        {
            // send exit messages
            for (actor_ptr& aptr : mlinks)
            {
                aptr->enqueue(this, make_tuple(atom(":Exit"), reason));
            }
        }
        for (attachable_ptr& ptr : mattachables)
        {
            ptr->detach(reason);
        }
    }

    bool link_to_impl(intrusive_ptr<actor>& other)
    {
        guard_type guard(m_mtx);
        if (other && !exited() && other->establish_backlink(this))
        {
            m_links.push_back(other);
            return true;
        }
        return false;
    }

    bool unlink_from_impl (intrusive_ptr<actor>& other)
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        if (other && !exited() && other->remove_backlink(this))
        {
            return erase_all(m_links, other) > 0;
        }
        return false;
    }

 public:

    bool attach(attachable* ptr) /*override*/
    {
        if (ptr == nullptr)
        {
            guard_type guard(m_mtx);
            return m_exit_reason.load() == exit_reason::not_exited;
        }
        else
        {
            attachable_ptr uptr(ptr);
            std::uint32_t reason;
            // lifetime scope of guard
            {
                guard_type guard(m_mtx);
                reason = m_exit_reason.load();
                if (reason == exit_reason::not_exited)
                {
                    m_attachables.push_back(std::move(uptr));
                    return true;
                }
            }
            uptr->detach(reason);
            return false;
        }
    }

    void detach(const attachable::token& what) /*override*/
    {
        attachable_ptr uptr;
        // lifetime scope of guard
        {
            guard_type guard(m_mtx);
            for (auto i = m_attachables.begin(); i != m_attachables.end(); ++i)
            {
                if ((*i)->matches(what))
                {
                    uptr = std::move(*i);
                    m_attachables.erase(i);
                    // exit loop (and release lock)
                    break;
                }
            }
        }
        // uptr will be destroyed here, without locked mutex
    }

    void link_to(intrusive_ptr<actor>& other) /*override*/
    {
        (void) link_to_impl(other);
    }

    void unlink_from(intrusive_ptr<actor>& other) /*override*/
    {
        (void) unlink_from_impl(other);
    }

    bool remove_backlink(intrusive_ptr<actor>& other) /*override*/
    {
        if (other && other != this)
        {
            std::lock_guard<std::mutex> guard(m_mtx);
            return erase_all(m_links, other) > 0;//m_links.erase(other) > 0;
        }
        return false;
    }

    bool establish_backlink(intrusive_ptr<actor>& other) /*override*/
    {
        bool result = false;
        std::uint32_t reason = exit_reason::not_exited;
        if (other && other != this)
        {
            // lifetime scope of guard
            {
                std::lock_guard<std::mutex> guard(m_mtx);
                reason = m_exit_reason.load();
                if (reason == exit_reason::not_exited)
                {
                    result = unique_insert(m_links, other);
                    //result = m_links.insert(other).second;
                }
            }
        }
        if (reason != exit_reason::not_exited)
        {
            other->enqueue(this, make_tuple(atom(":Exit"), reason));
        }
        return result;
    }

};

} } // namespace cppa::detail

#endif // ABSTRACT_ACTOR_HPP
