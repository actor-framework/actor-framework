/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_ACTOR_HPP
#define CPPA_ACTOR_HPP

#include <cstddef>
#include <cstdint>
#include <utility>
#include <type_traits>

#include "cppa/intrusive_ptr.hpp"
#include "cppa/abstract_actor.hpp"

#include "cppa/util/comparable.hpp"
#include "cppa/util/type_traits.hpp"

namespace cppa {

class actor_addr;
class actor_proxy;
class local_actor;
class blocking_actor;
class actor_companion;
class event_based_actor;

struct invalid_actor_addr_t;

namespace io {
class broker;
} // namespace io

namespace opencl {
template <typename Signature>
class actor_facade;
} // namespace opencl

namespace detail {
class raw_access;
} // namespace detail

struct invalid_actor_t { constexpr invalid_actor_t() { } };

/**
 * @brief Identifies an invalid {@link actor}.
 * @relates actor
 */
constexpr invalid_actor_t invalid_actor = invalid_actor_t{};

template<typename T>
struct is_convertible_to_actor {
    static constexpr bool value =  std::is_base_of<io::broker, T>::value
                                || std::is_base_of<actor_proxy, T>::value
                                || std::is_base_of<blocking_actor, T>::value
                                || std::is_base_of<actor_companion, T>::value
                                || std::is_base_of<event_based_actor, T>::value;
};

template<typename T>
struct is_convertible_to_actor<opencl::actor_facade<T>> {
    static constexpr bool value = true;
};

/**
 * @brief Identifies an untyped actor.
 *
 * Can be used with derived types of {@link event_based_actor},
 * {@link blocking_actor}, {@link actor_proxy}, or
 * {@link io::broker}.
 */
class actor : util::comparable<actor>
            , util::comparable<actor, actor_addr>
            , util::comparable<actor, invalid_actor_t>
            , util::comparable<actor, invalid_actor_addr_t> {

    friend class local_actor;
    friend class detail::raw_access;

 public:

    actor() = default;

    actor(actor&&) = default;

    actor(const actor&) = default;

    template<typename T>
    actor(intrusive_ptr<T> ptr,
          typename std::enable_if<is_convertible_to_actor<T>::value>::type* = 0)
            : m_ptr(std::move(ptr)) { }

    template<typename T>
    actor(T* ptr,
          typename std::enable_if<is_convertible_to_actor<T>::value>::type* = 0)
                : m_ptr(ptr) { }

    actor(const invalid_actor_t&);

    actor& operator=(actor&&) = default;

    actor& operator=(const actor&) = default;

    template<typename T>
    typename std::enable_if<is_convertible_to_actor<T>::value, actor&>::type
    operator=(intrusive_ptr<T> ptr) {
        actor tmp{std::move(ptr)};
        swap(tmp);
        return *this;
    }

    template<typename T>
    typename std::enable_if<is_convertible_to_actor<T>::value, actor&>::type
    operator=(T* ptr) {
        actor tmp{ptr};
        swap(tmp);
        return *this;
    }

    actor& operator=(const invalid_actor_t&);

    inline operator bool() const {
        return static_cast<bool>(m_ptr);
    }

    inline bool operator!() const {
        return !m_ptr;
    }

    /**
     * @brief Returns a handle that grants access to
     *        actor operations such as enqueue.
     */
    inline abstract_actor* operator->() const {
        return m_ptr.get();
    }

    inline abstract_actor& operator*() const {
        return *m_ptr;
    }

    intptr_t compare(const actor& other) const;

    intptr_t compare(const actor_addr&) const;

    inline intptr_t compare(const invalid_actor_t&) const {
        return m_ptr ? 1 : 0;
    }

    inline intptr_t compare(const invalid_actor_addr_t&) const {
        return compare(invalid_actor);
    }

    /**
     * @brief Queries the address of the stored actor.
     */
    actor_addr address() const;

 private:

    void swap(actor& other);

    actor(abstract_actor*);

    abstract_actor_ptr m_ptr;

};

} // namespace cppa

#endif // CPPA_ACTOR_HPP
