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


#include <tuple>
#include <cerrno>
#include <memory>
#include <cstring>
#include <sstream>
#include <iostream>
#include <stdexcept>

#include "cppa/on.hpp"
#include "cppa/actor.hpp"
#include "cppa/match.hpp"
#include "cppa/config.hpp"
#include "cppa/logging.hpp"
#include "cppa/to_string.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/uniform_type_info.hpp"
#include "cppa/thread_mapped_actor.hpp"
#include "cppa/binary_deserializer.hpp"
#include "cppa/process_information.hpp"

#include "cppa/util/buffer.hpp"

#include "cppa/network/protocol.hpp"
#include "cppa/network/acceptor.hpp"
#include "cppa/network/middleman.hpp"
#include "cppa/network/input_stream.hpp"
#include "cppa/network/output_stream.hpp"
#include "cppa/network/default_protocol.hpp"
#include "cppa/network/middleman_event_handler.hpp"

#include "cppa/detail/fd_util.hpp"
#include "cppa/detail/actor_registry.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

using namespace std;

namespace cppa { namespace network {

class middleman_event {

    friend class intrusive::single_reader_queue<middleman_event>;

 public:

    template<typename Arg>
    middleman_event(Arg&& arg) : next(nullptr), fun(forward<Arg>(arg)) { }

    inline void operator()() { fun(); }

 private:

    middleman_event* next;
    function<void()> fun;

};

typedef intrusive::single_reader_queue<middleman_event> middleman_queue;

class middleman_impl {

    friend class middleman;
    friend void middleman_loop(middleman_impl*);

 public:

    middleman_impl(std::unique_ptr<protocol>&& proto)
    : m_done(false), m_handler(middleman_event_handler::create())
    , m_protocol(std::move(proto)) { }

    protocol* get_protocol() {
        return m_protocol.get();
    }

    void run_later(function<void()> fun) {
        m_queue.enqueue(new middleman_event(move(fun)));
        atomic_thread_fence(memory_order_seq_cst);
        uint8_t dummy = 0;
        // ignore result; write error only means middleman already exited
        static_cast<void>(write(m_pipe_write, &dummy, sizeof(dummy)));
    }

    void continue_writer(const continuable_io_ptr& ptr) {
        CPPA_LOG_TRACE("ptr = " << ptr.get());
        m_handler->add_later(ptr, event::write);
    }

    void stop_writer(const continuable_io_ptr& ptr) {
        CPPA_LOG_TRACE("ptr = " << ptr.get());
        m_handler->erase_later(ptr, event::write);
    }

    void continue_reader(const continuable_io_ptr& ptr) {
        CPPA_LOG_TRACE("ptr = " << ptr.get());
        m_readers.push_back(ptr);
        m_handler->add_later(ptr, event::read);
    }

    void stop_reader(const continuable_io_ptr& ptr) {
        CPPA_LOG_TRACE("ptr = " << ptr.get());
        m_handler->erase_later(ptr, event::read);
        auto last = m_readers.end();
        auto i = find_if(m_readers.begin(), last, [&](const continuable_io_ptr& lhs) {
            return lhs == ptr;
        });
        if (i != last) m_readers.erase(i);
    }

 protected:

    void initialize() {
        int pipefds[2];
        if (pipe(pipefds) != 0) { CPPA_CRITICAL("cannot create pipe"); }
        m_pipe_read = pipefds[0];
        m_pipe_write = pipefds[1];
        detail::fd_util::nonblocking(m_pipe_read, true);
        // start threads
        m_thread = thread([this] { middleman_loop(this); });
    }

    void destroy() {
        run_later([this] {
            CPPA_LOGM_TRACE("destroy$helper", "");
            this->m_done = true;
        });
        m_thread.join();
        close(m_pipe_read);
        close(m_pipe_write);
    }

 private:

    inline void quit() { m_done = true; }

    inline bool done() const { return m_done; }

    bool m_done;
    std::vector<continuable_io_ptr> m_readers;

    middleman_event_handler& handler();

    thread m_thread;
    native_socket_type m_pipe_read;
    native_socket_type m_pipe_write;
    middleman_queue m_queue;
    std::unique_ptr<middleman_event_handler> m_handler;

    std::unique_ptr<protocol> m_protocol;

};

void middleman::set_pimpl(std::unique_ptr<protocol>&& proto) {
    m_impl.reset(new middleman_impl(std::move(proto)));
}

middleman* middleman::create_singleton() {
    auto ptr = new middleman;
    ptr->set_pimpl(std::unique_ptr<protocol>{new default_protocol(ptr)});
    return ptr;
}

class middleman_overseer : public continuable_io {

    typedef continuable_io super;

 public:

    middleman_overseer(int pipe_fd, middleman_queue& q)
    : super(pipe_fd), m_queue(q) { }

    continue_reading_result continue_reading() {
        CPPA_LOG_TRACE("");
        static constexpr size_t num_dummies = 64;
        uint8_t dummies[num_dummies];
        auto read_result = ::read(read_handle(), dummies, num_dummies);
        CPPA_LOGMF(CPPA_DEBUG, self, "read " << read_result << " messages from queue");
        if (read_result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // try again later
                return read_continue_later;
            }
            else {
                CPPA_LOGMF(CPPA_ERROR, self, "cannot read from pipe");
                CPPA_CRITICAL("cannot read from pipe");
            }
        }
        atomic_thread_fence(memory_order_seq_cst);
        for (int i = 0; i < read_result; ++i) {
            unique_ptr<middleman_event> msg(m_queue.try_pop());
            if (!msg) {
                CPPA_LOGMF(CPPA_ERROR, self, "nullptr dequeued");
                CPPA_CRITICAL("nullptr dequeued");
            }
            CPPA_LOGMF(CPPA_DEBUG, self, "execute run_later functor");
            (*msg)();
        }
        return read_continue_later;
    }

    void io_failed() { CPPA_CRITICAL("IO on pipe failed"); }

 private:

    middleman_queue& m_queue;

};

middleman::~middleman() { }

void middleman::destroy() {
    m_impl->destroy();
}

void middleman::initialize() {
    m_impl->initialize();
}

protocol* middleman::get_protocol() {
    return m_impl->get_protocol();
}

void middleman::run_later(std::function<void()> fun) {
    m_impl->run_later(std::move(fun));
}

void middleman::continue_writer(const continuable_io_ptr& ptr) {
    m_impl->continue_writer(ptr);
}

void middleman::stop_writer(const continuable_io_ptr& ptr) {
    m_impl->stop_writer(ptr);
}

void middleman::continue_reader(const continuable_io_ptr& ptr) {
    m_impl->continue_reader(ptr);
}

void middleman::stop_reader(const continuable_io_ptr& ptr) {
    m_impl->stop_reader(ptr);
}

void middleman_loop(middleman_impl* impl) {
#   ifdef CPPA_LOG_LEVEL
    auto mself = make_counted<thread_mapped_actor>();
    scoped_self_setter sss(mself.get());
    CPPA_SET_DEBUG_NAME("middleman");
#   endif
    middleman_event_handler* handler = impl->m_handler.get();
    CPPA_LOGF_TRACE("run middleman loop");
    CPPA_LOGF_INFO("middleman runs at "
                   << to_string(*process_information::get()));
    handler->init();
    impl->continue_reader(make_counted<middleman_overseer>(impl->m_pipe_read, impl->m_queue));
    handler->update();
    while (!impl->done()) {
        handler->poll([&](event_bitmask mask, continuable_io* io) {
            switch (mask) {
                default: CPPA_CRITICAL("invalid event");
                case event::none: break;
                case event::both:
                case event::write: {
                    CPPA_LOGF_DEBUG("handle event::write for " << io);
                    switch (io->continue_writing()) {
                        case write_closed:
                        case write_failure:
                            impl->stop_writer(io);
                            CPPA_LOGF_DEBUG("writer removed because of error");
                            break;
                        case write_done:
                            impl->stop_writer(io);
                            break;
                        default: break;
                    }
                    if (mask == event::write) break;
                    // else: fall through
                    CPPA_LOGF_DEBUG("handle event::both; fall through");
                }
                case event::read: {
                    CPPA_LOGF_DEBUG("handle event::read for " << io);
                    switch (io->continue_reading()) {
                        case read_closed:
                        case read_failure:
                            impl->stop_reader(io);
                            CPPA_LOGF_DEBUG("remove peer");
                            break;
                        default: break;
                    }
                    break;
                }
                case event::error: {
                    CPPA_LOGF_DEBUG("event::error; remove peer " << io);
                    io->io_failed();
                    impl->stop_reader(io);
                    impl->stop_writer(io);
                }
            }
        });
    }
    CPPA_LOGF_DEBUG("event loop done, erase all readers");
    // make sure to write everything before shutting down
    for (auto ptr : impl->m_readers) { handler->erase_later(ptr, event::read); }
    handler->update();
    CPPA_LOGF_DEBUG("flush outgoing messages");
    CPPA_LOGF_DEBUG_IF(handler->num_sockets() == 0,
                       "nothing to flush, no writer left");
    while (handler->num_sockets() > 0) {
        handler->poll([&](event_bitmask mask, continuable_io* io) {
            switch (mask) {
                case event::write:
                    switch (io->continue_writing()) {
                        case write_closed:
                        case write_failure:
                        case write_done:
                            handler->erase_later(io, event::write);
                            break;
                        default: break;
                    }
                    break;
                case event::error:
                    io->io_failed();
                    handler->erase_later(io, event::both);
                    break;
                default:
                    CPPA_LOGF_ERROR("expected event::write only "
                                    "during shutdown phase");
                    handler->erase_later(io, event::read);
                    break;
            }
        });
    }
    CPPA_LOGF_DEBUG("clear all containers");
    //impl->m_peers.clear();
    impl->m_readers.clear();
    CPPA_LOGF_DEBUG("middleman loop done");
}

} } // namespace cppa::detail
