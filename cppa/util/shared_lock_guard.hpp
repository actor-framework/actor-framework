#ifndef SHARED_LOCK_GUARD_HPP
#define SHARED_LOCK_GUARD_HPP

namespace cppa { namespace util {

template<typename SharedLockable>
class shared_lock_guard
{

    SharedLockable* m_lockable;

 public:

    explicit shared_lock_guard(SharedLockable& lockable) : m_lockable(&lockable)
    {
        m_lockable->lock_shared();
    }

    ~shared_lock_guard()
    {
        if (m_lockable) m_lockable->unlock_shared();
    }

    bool owns_lock() const
    {
        return m_lockable != nullptr;
    }

    SharedLockable* release()
    {
        auto result = m_lockable;
        m_lockable = nullptr;
        return result;
    }
};

} } // namespace cppa::util

#endif // SHARED_LOCK_GUARD_HPP
