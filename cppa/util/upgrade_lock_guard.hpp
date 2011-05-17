#ifndef UPGRADE_LOCK_GUARD_HPP
#define UPGRADE_LOCK_GUARD_HPP

namespace cppa { namespace util {

/**
 * @brief Upgrades shared ownership to exclusive ownership.
 */
template<typename UpgradeLockable>
class upgrade_lock_guard
{

    UpgradeLockable* m_lockable;

 public:

    template<template <typename> class LockType>
    upgrade_lock_guard(LockType<UpgradeLockable>& other)
    {
        m_lockable = other.release();
        if (m_lockable) m_lockable->lock_upgrade();
    }

    ~upgrade_lock_guard()
    {
        if (m_lockable) m_lockable->unlock();
    }

    bool owns_lock() const
    {
        return m_lockable != nullptr;
    }

};

} } // namespace cppa::util

#endif // UPGRADE_LOCK_GUARD_HPP
