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


#include <poll.h>
#include "cppa/network/middleman_event_handler.hpp"

#ifndef POLLRDHUP
#define POLLRDHUP POLLHUP
#endif

namespace cppa { namespace network {

namespace {

bool pollfd_less(const pollfd& lhs, native_socket_type rhs) {
    return lhs.fd < rhs;
}

short to_poll_bitmask(event_bitmask mask) {
    switch (mask) {
        case event::read:  return POLLIN;
        case event::write: return POLLOUT;
        case event::both:  return (POLLIN|POLLOUT);
        default: CPPA_CRITICAL("invalid event bitmask");
    }
}

class middleman_event_handler_impl : public middleman_event_handler {

 public:

    void init() { }

 protected:

    void poll_impl() {
        CPPA_REQUIRE(m_pollset.empty() == false);
        CPPA_REQUIRE(m_pollset.size() == m_meta.size());
        int presult;
        do {
            presult = ::poll(m_pollset.data(), m_pollset.size(), -1);
            CPPA_LOG_DEBUG("poll() on " << num_sockets()
                           << " sockets returned " << presult);
            if (presult < 0) {
                switch (errno) {
                    // a signal was caught
                    case EINTR: {
                        // just try again
                        break;
                    }
                    case ENOMEM: {
                        CPPA_LOGMF(CPPA_ERROR, self, "poll() failed for reason ENOMEM");
                        // there's not much we can do other than try again
                        // in hope someone releases memory
                        //this_thread::yield();
                        break;
                    }
                    default: {
                        perror("poll() failed");
                        CPPA_CRITICAL("poll() failed");
                    }
                }
            }
            else {
                for (size_t i = 0; i < m_pollset.size(); ++i) {
                    event_bitmask eb = event::none;
                    auto& revents = m_pollset[i].revents;
                    // read as long as possible, ignore POLLHUP as long as
                    // there is still data available
                    if (revents & (POLLIN | POLLPRI)) eb |= event::read;
                    else if (revents & (POLLRDHUP | POLLERR | POLLHUP | POLLNVAL)) {
                        CPPA_LOG_DEBUG_IF(revents & POLLRDHUP, "POLLRDHUP");
                        CPPA_LOG_DEBUG_IF(revents & POLLERR,   "POLLERR");
                        CPPA_LOG_DEBUG_IF(revents & POLLHUP,   "POLLHUP");
                        CPPA_LOG_DEBUG_IF(revents & POLLNVAL,  "POLLNVAL");
                        eb = event::error;
                    }
                    // POLLOUT and POLLHUP are mutually exclusive:
                    // no need to check wheter event::error has been set
                    if (revents & POLLOUT) eb |= event::write;
                    revents = 0;
                    m_events.emplace_back(eb, m_meta[i].ptr.get());
                }
            }
        } while (presult < 0);
    }

    void handle_event(fd_meta_event me,
                      native_socket_type fd,
                      event_bitmask,
                      event_bitmask new_bitmask,
                      continuable_io*) {
        auto last = m_pollset.end();
        auto iter = std::lower_bound(m_pollset.begin(), last, fd, pollfd_less);
        switch (me) {
            case fd_meta_event::add: {
                pollfd tmp;
                tmp.fd = fd;
                tmp.events = to_poll_bitmask(new_bitmask);
                tmp.revents = 0;
                m_pollset.insert(iter, tmp);
                CPPA_LOGMF(CPPA_DEBUG, self, "inserted new element");
                break;
            }
            case fd_meta_event::erase: {
                CPPA_LOG_ERROR_IF(iter == last || iter->fd != fd,
                                  "m_meta and m_pollset out of sync; "
                                  "no element found for fd (cannot erase)");
                if (iter != last && iter->fd == fd) {
                    CPPA_LOGMF(CPPA_DEBUG, self, "erased element");
                    m_pollset.erase(iter);
                }
                break;
            }
            case fd_meta_event::mod: {
                CPPA_LOG_ERROR_IF(iter == last || iter->fd != fd,
                                  "m_meta and m_pollset out of sync; "
                                  "no element found for fd (cannot erase)");
                if (iter != last && iter->fd == fd) {
                    CPPA_LOGMF(CPPA_DEBUG, self, "updated bitmask");
                    iter->events = to_poll_bitmask(new_bitmask);
                }
                break;
            }
        }
    }

 private:

    std::vector<pollfd> m_pollset; // always in sync with m_meta

};

} // namespace <anonymous>

std::unique_ptr<middleman_event_handler> middleman_event_handler::create() {
    return std::unique_ptr<middleman_event_handler>{new middleman_event_handler_impl};
}

} } // namespace cppa::network
