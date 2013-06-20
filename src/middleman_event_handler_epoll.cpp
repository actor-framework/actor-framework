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

#include <ios>
#include <string>
#include <vector>

#include <string.h>
#include <sys/epoll.h>

#include "cppa/io/middleman_event_handler.hpp"

namespace cppa { namespace io {

namespace {

static constexpr unsigned input_event  = EPOLLIN;
static constexpr unsigned error_event  = EPOLLRDHUP | EPOLLERR | EPOLLHUP;
static constexpr unsigned output_event = EPOLLOUT;

class middleman_event_handler_impl : public middleman_event_handler {

 public:

    middleman_event_handler_impl() : m_epollfd(-1) { }

    ~middleman_event_handler_impl() { if (m_epollfd != -1) close(m_epollfd); }

    void init() {
        m_epollfd = epoll_create1(EPOLL_CLOEXEC);
        if (m_epollfd == -1) {
            throw std::ios_base::failure(  std::string("epoll_create1: ")
                                         + strerror(errno));
        }
        // handle at most 64 events at a time
        m_epollset.resize(64);
    }

 protected:

    void poll_impl() {
        CPPA_REQUIRE(m_meta.empty() == false);
        int presult = -1;
        while (presult < 0) {
            presult = epoll_wait(m_epollfd,
                                 m_epollset.data(),
                                 static_cast<int>(m_epollset.size()),
                                 -1);
            CPPA_LOG_DEBUG("epoll_wait on " << num_sockets()
                           << " sockets returned " << presult);
            if (presult < 0) {
                switch (errno) {
                    case EINTR: {
                        // a signal was caught
                        // just try again
                        break;
                    }
                    default: {
                        perror("epoll_wait() failed");
                        CPPA_CRITICAL("epoll_wait() failed");
                    }
                }
            }
        }
        auto iter = m_epollset.begin();
        auto last = iter + static_cast<size_t>(presult);
        for ( ; iter != last; ++iter) {
            auto eb = from_int_bitmask<input_event,
                                       output_event,
                                       error_event>(iter->events);
            auto ptr = reinterpret_cast<continuable*>(iter->data.ptr);
            CPPA_REQUIRE(eb != event::none);
            m_events.emplace_back(eb, ptr);
        }
    }

    void handle_event(fd_meta_event me,
                      native_socket_type fd,
                      event_bitmask,
                      event_bitmask new_bitmask,
                      continuable* ptr) {
        int operation;
        epoll_event ee;
        ee.data.ptr = ptr;
        switch (new_bitmask) {
            case event::none:
                CPPA_REQUIRE(me == fd_meta_event::erase);
                ee.events = 0;
                break;
            case event::read:
                ee.events = EPOLLIN | EPOLLRDHUP;
                break;
            case event::write:
                ee.events = EPOLLOUT;
            case event::both:
                ee.events = EPOLLIN | EPOLLRDHUP | EPOLLOUT;
                break;
            default: CPPA_CRITICAL("invalid event bitmask");
        }
        switch (me) {
            case fd_meta_event::add:
                operation = EPOLL_CTL_ADD;
                break;
            case fd_meta_event::erase:
                operation = EPOLL_CTL_DEL;
                break;
            case fd_meta_event::mod:
                operation = EPOLL_CTL_MOD;
                break;
            default: CPPA_CRITICAL("invalid fd_meta_event");
        }
        if (epoll_ctl(m_epollfd, operation, fd, &ee) < 0) {
            switch (errno) {
                // supplied file descriptor is already registered
                case EEXIST: {
                    CPPA_LOGMF(CPPA_ERROR, self, "file descriptor registered twice");
                    break;
                }
                // op was EPOLL_CTL_MOD or EPOLL_CTL_DEL,
                // and fd is not registered with this epoll instance.
                case ENOENT: {
                    CPPA_LOGMF(CPPA_ERROR, self, "cannot delete file descriptor "
                                   "because it isn't registered");
                    break;
                }
                default: {
                    CPPA_LOGMF(CPPA_ERROR, self, strerror(errno));
                    perror("epoll_ctl() failed");
                    CPPA_CRITICAL("epoll_ctl() failed");
                }
            }
        }
    }

 private:

    int m_epollfd;
    std::vector<epoll_event> m_epollset;

};

} // namespace <anonymous>

std::unique_ptr<middleman_event_handler> middleman_event_handler::create() {
    return std::unique_ptr<middleman_event_handler>{new middleman_event_handler_impl};
}

} } // namespace cppa::network
