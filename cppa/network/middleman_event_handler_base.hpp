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
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef MIDDLEMAN_EVENT_HANDLER_BASE_HPP
#define MIDDLEMAN_EVENT_HANDLER_BASE_HPP

#include <vector>
#include <utility>

#include "cppa/config.hpp"
#include "cppa/logging.hpp"

#include "cppa/network/continuable_reader.hpp"
#include "cppa/network/continuable_io.hpp"

namespace cppa { namespace network {

typedef int event_bitmask;

namespace event { namespace {

constexpr event_bitmask none  = 0x00;
constexpr event_bitmask read  = 0x01;
constexpr event_bitmask write = 0x02;
constexpr event_bitmask both  = 0x03;
constexpr event_bitmask error = 0x04;

} } // namespace event

/**
 * @brief Converts an event bitmask to a human-readable string.
 */
inline const char* eb2str(event_bitmask e) {
    switch (e) {
        default: return "INVALID";
        case event::none:  return "event::none";
        case event::read:  return "event::read";
        case event::write: return "event::write";
        case event::both:  return "event::both";
        case event::error: return "event::error";
    }
}

struct fd_meta_info {
    native_socket_type fd;
    continuable_reader_ptr ptr;
    event_bitmask mask;
    fd_meta_info(native_socket_type a0,
                 const continuable_reader_ptr& a1,
                 event_bitmask a2)
    : fd(a0), ptr(a1), mask(a2) { }
};

struct fd_meta_info_less {
    inline bool operator()(const fd_meta_info& lhs, native_socket_type rhs) const {
        return lhs.fd < rhs;
    }
};

enum class fd_meta_event { add, erase, mod };

template<typename Derived>
class middleman_event_handler_base {

 public:

    typedef std::vector<fd_meta_info> vector_type;

    virtual ~middleman_event_handler_base() { }

    void alteration(const continuable_reader_ptr& ptr,
                    event_bitmask e,
                    fd_meta_event etype) {
        native_socket_type fd;
        switch (e) {
            case event::read:
                fd = ptr->read_handle();
                break;
            case event::write: {
                auto wptr = ptr->as_io();
                if (wptr) fd = wptr->write_handle();
                else {
                    CPPA_LOG_ERROR("ptr->downcast() returned nullptr");
                    return;
                }
                break;
            }
            case event::both: {
                fd = ptr->read_handle();
                auto wptr = ptr->as_io();
                if (wptr) {
                    auto wrfd = wptr->write_handle();
                    if (fd != wrfd) {
                        CPPA_LOG_DEBUG("read_handle != write_handle, split "
                                       "into two function calls");
                        // split into two function calls
                        e = event::read;
                        alteration(ptr, event::write, etype);
                    }
                }
                else {
                    CPPA_LOG_ERROR("ptr->downcast() returned nullptr");
                    return;
                }
                break;
            }
            default:
                CPPA_LOG_ERROR("invalid bitmask");
                return;
        }
        m_alterations.emplace_back(fd_meta_info(fd, ptr, e), etype);
    }

    void add(const continuable_reader_ptr& ptr, event_bitmask e) {
        CPPA_LOG_TRACE("ptr = " << ptr.get() << ", e = " << eb2str(e));
        alteration(ptr, e, fd_meta_event::add);
    }

    void erase(const continuable_reader_ptr& ptr, event_bitmask e) {
        CPPA_LOG_TRACE("ptr = " << ptr.get() << ", e = " << eb2str(e));
        alteration(ptr, e, fd_meta_event::erase);
    }

    inline event_bitmask next_bitmask(event_bitmask old, event_bitmask arg, fd_meta_event op) {
        CPPA_REQUIRE(op == fd_meta_event::add || op == fd_meta_event::erase);
        return (op == fd_meta_event::add) ? old | arg : old & ~arg;
    }

    void update() {
        CPPA_LOG_TRACE("");
        for (auto& elem_pair : m_alterations) {
            auto& elem = elem_pair.first;
            auto old = event::none;
            auto last = end(m_meta);
            auto iter = lower_bound(begin(m_meta), last, elem.fd, m_less);
            if (iter != last) old = iter->mask;
            auto mask = next_bitmask(old, elem.mask, elem_pair.second);
            auto ptr = elem.ptr.get();
            CPPA_LOG_DEBUG("new bitmask for "
                           << elem.ptr.get() << ": " << eb2str(mask));
            if (iter == last || iter->fd != elem.fd) {
                CPPA_LOG_INFO_IF(mask == event::none,
                                 "cannot erase " << ptr
                                 << " (not found in m_meta)");
                if (mask != event::none) {
                    m_meta.insert(iter, elem);
                    d()->handle_event(fd_meta_event::add, elem.fd,
                                 event::none, mask, ptr);
                }
            }
            else if (iter->fd == elem.fd) {
                CPPA_REQUIRE(iter->ptr == elem.ptr);
                if (mask == event::none) {
                    m_meta.erase(iter);
                    d()->handle_event(fd_meta_event::erase, elem.fd, old, mask, ptr);
                }
                else {
                    iter->mask = mask;
                    d()->handle_event(fd_meta_event::mod, elem.fd, old, mask, ptr);
                }
            }
        }
        m_alterations.clear();
    }

 protected:

    fd_meta_info_less m_less;
    vector_type m_meta; // this vector is *always* sorted

    std::vector<std::pair<fd_meta_info,fd_meta_event> > m_alterations;

 private:

    vector_type::iterator find_meta(native_socket_type fd) {
        auto last = end(m_meta);
        auto iter = lower_bound(begin(m_meta), last, fd, m_less);
        return (iter != last && iter->fd == fd) ? iter : last;
    }

    Derived* d() { return static_cast<Derived*>(this); }

};

template<class BaseIter, class BasIterAccess>
class event_iterator_impl {

 public:

    event_iterator_impl(const BaseIter& iter) : m_i(iter) { }

    inline event_iterator_impl& operator++() {
        m_access.advance(m_i);
        return *this;
    }

    inline event_iterator_impl* operator->() { return this; }

    inline const event_iterator_impl* operator->() const { return this; }

    inline event_bitmask type() const {
        return m_access.type(m_i);
    }

    inline void io_failed() {
        ptr()->io_failed();
    }

    inline continue_reading_result continue_reading() {
        return ptr()->continue_reading();
    }

    inline continue_writing_result continue_writing() {
        return ptr()->as_io()->continue_writing();
    }

    inline bool equal_to(const event_iterator_impl& other) const {
        return m_access.equal(m_i, other.m_i);
    }

    inline void handled() { m_access.handled(m_i); }

    inline continuable_reader* ptr() { return m_access.ptr(m_i); }

 private:

    BaseIter m_i;
    BasIterAccess m_access;

};

template<class Iter, class Access>
inline bool operator==(const event_iterator_impl<Iter,Access>& lhs,
                       const event_iterator_impl<Iter,Access>& rhs) {
    return lhs.equal_to(rhs);
}

template<class Iter, class Access>
inline bool operator!=(const event_iterator_impl<Iter,Access>& lhs,
                       const event_iterator_impl<Iter,Access>& rhs) {
    return !lhs.equal_to(rhs);
}

} } // namespace cppa::detail

#endif // MIDDLEMAN_EVENT_HANDLER_BASE_HPP
