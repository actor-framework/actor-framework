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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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


#ifndef MIDDLEMAN_EVENT_HANDLER_HPP
#define MIDDLEMAN_EVENT_HANDLER_HPP

#include <vector>
#include <utility>

#include "cppa/config.hpp"
#include "cppa/logging.hpp"

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
    continuable_io_ptr ptr;
    event_bitmask mask;
    fd_meta_info(native_socket_type a0,
                 const continuable_io_ptr& a1,
                 event_bitmask a2)
    : fd(a0), ptr(a1), mask(a2) { }
};

struct fd_meta_info_less {
    inline bool operator()(const fd_meta_info& lhs, native_socket_type rhs) const {
        return lhs.fd < rhs;
    }
};

enum class fd_meta_event { add, erase, mod };

class middleman_event_handler {

 public:

    typedef std::pair<event_bitmask, continuable_io*> io_event;

    virtual ~middleman_event_handler();

    void alteration(const continuable_io_ptr& ptr, event_bitmask e, fd_meta_event etype);

    void add(const continuable_io_ptr& ptr, event_bitmask e);

    void erase(const continuable_io_ptr& ptr, event_bitmask e);

    event_bitmask next_bitmask(event_bitmask old, event_bitmask arg, fd_meta_event op) const;

    template<typename F>
    void poll(const F& fun) {
        poll_impl();
        for (auto& p : m_events) fun(p.first, p.second);
        m_events.clear();
        update();
    }

    // pure virtual member function

    virtual void init() = 0;

    virtual size_t num_sockets() const = 0;

    // fills the event vector
    virtual void poll_impl() = 0;

    virtual void handle_event(fd_meta_event me,
                              native_socket_type fd,
                              event_bitmask old_bitmask,
                              event_bitmask new_bitmask,
                              continuable_io* ptr) = 0;

    // implemented in platform-dependent .cpp file
    static std::unique_ptr<middleman_event_handler> create();

    void update();

 protected:

    fd_meta_info_less m_less;
    std::vector<fd_meta_info> m_meta; // this vector is *always* sorted

    std::vector<std::pair<fd_meta_info, fd_meta_event> > m_alterations;

    std::vector<io_event> m_events;

    middleman_event_handler();

 private:

    std::vector<fd_meta_info>::iterator find_meta(native_socket_type fd) {
        auto last = end(m_meta);
        auto iter = lower_bound(begin(m_meta), last, fd, m_less);
        return (iter != last && iter->fd == fd) ? iter : last;
    }

};

} } // namespace cppa::detail

#endif // MIDDLEMAN_EVENT_HANDLER_HPP
