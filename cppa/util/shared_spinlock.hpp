#ifndef SHARED_SPINLOCK_HPP
#define SHARED_SPINLOCK_HPP

#include <atomic>
#include <cstddef>

namespace cppa { namespace util {

class shared_spinlock
{

    std::atomic<long> m_flag;

 public:

    shared_spinlock();

    void lock();
    void unlock();
    bool try_lock();

    void lock_shared();
    void unlock_shared();
    bool try_lock_shared();

    /**
     * @brief Upgrades a shared lock to an exclusive lock (must be released
     *        through a call to {@link lock()} afterwards).
     */
    void lock_upgrade();

};

} } // namespace cppa::util

#endif // SHARED_SPINLOCK_HPP
