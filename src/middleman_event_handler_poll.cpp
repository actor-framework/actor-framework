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


#include "cppa/config.hpp"

#if !defined(CPPA_LINUX) || defined(CPPA_POLL_IMPL)

#ifndef CPPA_WINDOWS
#include <poll.h>
#define SOCKERR errno 
#else
#   include <ws2tcpip.h>
#   include <ws2ipdef.h>
#   include <winsock2.h>
#define SOCKERR GetLastError()
#undef EINTR
#undef ENOMEM
#define EINTR WSAEINTR
#define ENOMEM WSAENOBUFS
#endif


#include "cppa/io/middleman_event_handler.hpp"

#ifndef POLLRDHUP
#define POLLRDHUP POLLHUP
#endif

namespace cppa { namespace io {

namespace {

static constexpr unsigned input_event  = POLLIN | POLLPRI;
static constexpr unsigned error_event  = POLLRDHUP | POLLERR | POLLHUP | POLLNVAL;
static constexpr unsigned output_event = POLLOUT;

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
        int presult = -1;
        while (presult < 0) {
#ifdef CPPA_WINDOWS 
            presult = ::WSAPoll(m_pollset.data(), m_pollset.size(), -1);
#else
            presult = ::poll(m_pollset.data(), m_pollset.size(), -1);
#endif
            CPPA_LOG_DEBUG("poll() on " << num_sockets()
                           << " sockets returned " << presult);
            if (presult < 0) {
                switch (SOCKERR) {
                    case EINTR: {
                        // a signal was caught
                        // just try again
                        break;
                    }
                    case ENOMEM: {
                        CPPA_LOG_ERROR("poll() failed for reason ENOMEM");
                        // there's not much we can do other than try again
                        // in hope someone else releases memory
                        break;
                    }
                    default: {
                        perror("poll() failed");
                        CPPA_CRITICAL("poll() failed");
                    }
                }
            }
        }
        for (size_t i = 0; i < m_pollset.size(); ++i) {
            auto mask = static_cast<unsigned>(m_pollset[i].revents);
            auto eb = from_int_bitmask<input_event,
                                       output_event,
                                       error_event>(mask);
            m_pollset[i].revents = 0;
            if (eb != event::none) m_events.emplace_back(eb, m_meta[i].ptr);
        }
    }

    void handle_event(fd_meta_event me,
                      native_socket_type fd,
                      event_bitmask,
                      event_bitmask new_bitmask,
                      continuable*) {
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

} } // namespace cppa::io

#else // !defined(CPPA_LINUX) || defined(CPPA_POLL_IMPL)

int keep_compiler_happy_for_poll_impl() { return 42; }

#endif // !defined(CPPA_LINUX) || defined(CPPA_POLL_IMPL)
