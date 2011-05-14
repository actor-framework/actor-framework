#ifndef SHARED_LOCK_GUARD_HPP
#define SHARED_LOCK_GUARD_HPP

namespace cppa { namespace util {

template<typename SharedLockable>
class shared_lock_guard
{

    SharedLockable& m_lockable;

 public:

    shared_lock_guard(SharedLockable& lockable) : m_lockable(lockable)
    {
        m_lockable.lock_shared();
    }

    ~shared_lock_guard()
    {
        m_lockable.unlock_shared();
    }

};

} } // namespace cppa::util

#endif // SHARED_LOCK_GUARD_HPP
