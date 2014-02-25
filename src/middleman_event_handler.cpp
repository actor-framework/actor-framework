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


#include "cppa/io/middleman_event_handler.hpp"

namespace cppa { namespace io {

inline std::string eb2str(event_bitmask e) {
    switch (e) {
        default: return "INVALID";
        case event::none:  return "event::none";
        case event::read:  return "event::read";
        case event::write: return "event::write";
        case event::both:  return "event::both";
        case event::error: return "event::error";
    }
}

middleman_event_handler::middleman_event_handler() { }

middleman_event_handler::~middleman_event_handler() { }

void middleman_event_handler::alteration(continuable* ptr,
                                         event_bitmask e,
                                         fd_meta_event etype) {
    native_socket_type fd;
    switch (e) {
        case event::read:
            fd = ptr->read_handle();
            break;
        case event::write: {
            fd = ptr->write_handle();
            break;
        }
        case event::both: {
            fd = ptr->read_handle();
            auto wrfd = ptr->write_handle();
            if (fd != wrfd) {
                CPPA_LOG_DEBUG("read_handle != write_handle, split "
                               "into two function calls");
                // split into two function calls
                e = event::read;
                alteration(ptr, event::write, etype);
            }
            break;
        }
        default:
            CPPA_CRITICAL("invalid bitmask");
            return;
    }
    m_alterations.emplace_back(fd_meta_info(fd, ptr, e), etype);
}

void middleman_event_handler::add_later(continuable* ptr, event_bitmask e) {
    CPPA_LOG_TRACE(CPPA_ARG(ptr) << ", "
                   << CPPA_TARG(e, eb2str)
                   << ", socket = " << ptr->read_handle());
    alteration(ptr, e, fd_meta_event::add);
}

void middleman_event_handler::erase_later(continuable* ptr, event_bitmask e) {
    CPPA_LOG_TRACE(CPPA_ARG(ptr) << ", e = " << eb2str(e));
    alteration(ptr, e, fd_meta_event::erase);
}

event_bitmask middleman_event_handler::next_bitmask(event_bitmask old, event_bitmask arg, fd_meta_event op) const {
    CPPA_REQUIRE(op == fd_meta_event::add || op == fd_meta_event::erase);
    return (op == fd_meta_event::add) ? old | arg : old & ~arg;
}

void middleman_event_handler::update() {
    CPPA_LOG_TRACE("");
    auto mless = [](const fd_meta_info& lhs, native_socket_type rhs) {
        return lhs.fd < rhs;
    };
    for (auto& elem_pair : m_alterations) {
        auto& elem = elem_pair.first;
        auto old = event::none;
        auto last = m_meta.end();
        auto iter = std::lower_bound(m_meta.begin(), last, elem.fd, mless);
        if (iter != last) old = iter->mask;
        auto mask = next_bitmask(old, elem.mask, elem_pair.second);
        auto ptr = elem.ptr;
        CPPA_LOG_DEBUG("new bitmask for "
                       << elem.ptr << ": " << eb2str(mask));
        if (iter == last || iter->fd != elem.fd) {
            CPPA_LOG_ERROR_IF(mask == event::none,
                              "cannot erase " << ptr << " (no such element)");
            if (mask != event::none) {
                m_meta.insert(iter, elem);
                handle_event(fd_meta_event::add, elem.fd,
                             event::none, mask, ptr);
            }
        }
        else if (iter->fd == elem.fd) {
            CPPA_REQUIRE(iter->ptr == elem.ptr);
            if (mask == event::none) {
                // note: we cannot decide whether it's safe to dispose `ptr`,
                // because we didn't parse all alterations yet
                m_dispose_list.emplace_back(ptr);
                m_meta.erase(iter);
                handle_event(fd_meta_event::erase, elem.fd, old, mask, ptr);
            }
            else {
                iter->mask = mask;
                handle_event(fd_meta_event::mod, elem.fd, old, mask, ptr);
            }
        }
    }
    m_alterations.clear();
    // m_meta won't be touched inside loop
    auto first = m_meta.begin();
    auto last = m_meta.end();
    auto is_alive = [&](native_socket_type fd) -> bool {
        auto iter = std::lower_bound(first, last, fd, mless);
        return iter != last && iter->fd == fd;
    };
    // check whether elements in dispose list can be safely deleted
    for (auto elem : m_dispose_list) {
        auto rd = elem->read_handle();
        auto wr = elem->write_handle();
        if  ( (rd == wr && !is_alive(rd))
           || (rd != wr && !is_alive(rd) && !is_alive(wr))) {
           elem->dispose();
        }
    }
    m_dispose_list.clear();
}

bool middleman_event_handler::has_reader(continuable* ptr) {
    return std::any_of(m_meta.begin(), m_meta.end(), [=](fd_meta_info& meta) {
        return meta.ptr == ptr && (meta.mask & event::read);
    });
}

bool middleman_event_handler::has_writer(continuable* ptr) {
    return std::any_of(m_meta.begin(), m_meta.end(), [=](fd_meta_info& meta) {
        return meta.ptr == ptr && (meta.mask & event::write);
    });
}

} } // namespace cppa::network
