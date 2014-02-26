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

#include "cppa/io/protocol.hpp"
#include "cppa/io/acceptor.hpp"
#include "cppa/io/middleman.hpp"
#include "cppa/io/input_stream.hpp"
#include "cppa/io/output_stream.hpp"
#include "cppa/io/default_protocol.hpp"
#include "cppa/io/middleman_event_handler.hpp"

#include "cppa/detail/fd_util.hpp"
#include "cppa/detail/actor_registry.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

#ifdef CPPA_WINDOWS
#include "io.h"
#include "fcntl.h"
#endif

using namespace std;

namespace cppa { namespace io {

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
#ifdef CPPA_WINDOWS 
        auto unused_ret = ::send(m_pipe_write, (char *)&dummy, sizeof(dummy), 0);
#else
        auto unused_ret = write(m_pipe_write, &dummy, sizeof(dummy));
#endif
        static_cast<void>(unused_ret);
    }

    void continue_writer(continuable* ptr) {
        CPPA_LOG_TRACE(CPPA_ARG(ptr));
        m_handler->add_later(ptr, event::write);
    }

    void stop_writer(continuable* ptr) {
        CPPA_LOG_TRACE(CPPA_ARG(ptr));
        m_handler->erase_later(ptr, event::write);
    }

    inline bool has_writer(continuable* ptr) {
        return m_handler->has_writer(ptr);
    }

    void continue_reader(continuable* ptr) {
        CPPA_LOG_TRACE(CPPA_ARG(ptr));
        m_handler->add_later(ptr, event::read);
    }

    void stop_reader(continuable* ptr) {
        CPPA_LOG_TRACE(CPPA_ARG(ptr));
        m_handler->erase_later(ptr, event::read);
    }

    inline bool has_reader(continuable* ptr) {
        return m_handler->has_reader(ptr);
    }

 protected:

    void initialize() {
#ifdef CPPA_WINDOWS
        // if ( CreatePipe(&m_pipe_read,&m_pipe_write,0,4096) ) { CPPA_CRITICAL("cannot create pipe"); }
        // DWORD dwMode = PIPE_NOWAIT; 
        // if ( !SetNamedPipeHandleState(&m_pipe_read,&dwMode,NULL,NULL) ) { CPPA_CRITICAL("cannot set PIPE_NOWAIT"); }
        native_socket_type pipefds[2];       
         if ( cppa::io::dumb_socketpair(pipefds, 0) != 0) { CPPA_CRITICAL("cannot create pipe"); }
#else
        int pipefds[2];
        if (pipe(pipefds) != 0) { CPPA_CRITICAL("cannot create pipe"); }
#endif
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

        closesocket(m_pipe_read);
        closesocket(m_pipe_write);
#
    }

 private:

    inline void quit() { m_done = true; }

    inline bool done() const { return m_done; }

    bool m_done;

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

class middleman_overseer : public continuable {

    typedef continuable super;

 public:

    middleman_overseer(native_socket_type pipe_fd, middleman_queue& q)
    : super(pipe_fd), m_queue(q) { }

    void dispose() override {
        delete this;
    }

    continue_reading_result continue_reading() {
        CPPA_LOG_TRACE("");
        static constexpr size_t num_dummies = 64;
        uint8_t dummies[num_dummies];
#ifdef CPPA_WINDOWS
        auto read_result = ::recv(read_handle(), (char *)dummies, num_dummies, 0);
#else
        auto read_result = ::read(read_handle(), dummies, num_dummies);
#endif
        CPPA_LOGMF(CPPA_DEBUG, self, "read " << read_result << " messages from queue");


#ifdef CPPA_WINDOWS
        if (read_result == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                // try again later
                return read_continue_later;
            }
            else {
                CPPA_LOGMF(CPPA_ERROR, self, "cannot read from pipe");
                CPPA_CRITICAL("cannot read from pipe");
            }
        }
#else
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
#endif
        for (int i = 0; i < read_result; ++i) {
            unique_ptr<middleman_event> msg(m_queue.try_pop());
            if (!msg) {
                CPPA_LOGMF(CPPA_ERROR, self, "nullptr dequeued");
                CPPA_CRITICAL("nullptr dequeued");
            }
            CPPA_LOGF_DEBUG("execute run_later functor");
            (*msg)();
        }
        return read_continue_later;
    }

    void io_failed(event_bitmask) override {
        CPPA_CRITICAL("IO on pipe failed");
    }

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

void middleman::continue_writer(continuable* ptr) {
    m_impl->continue_writer(ptr);
}

void middleman::stop_writer(continuable* ptr) {
    m_impl->stop_writer(ptr);
}

bool middleman::has_writer(continuable* ptr) {
    return m_impl->has_writer(ptr);
}

void middleman::continue_reader(continuable* ptr) {
    m_impl->continue_reader(ptr);
}

void middleman::stop_reader(continuable* ptr) {
    m_impl->stop_reader(ptr);
}

bool middleman::has_reader(continuable* ptr) {
    return m_impl->has_reader(ptr);
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
    impl->continue_reader(new middleman_overseer(impl->m_pipe_read, impl->m_queue));
    handler->update();
    while (!impl->done()) {
        handler->poll([&](event_bitmask mask, continuable* io) {
            switch (mask) {
                default: CPPA_CRITICAL("invalid event");
                case event::none: break;
                case event::both:
                case event::write: {
                    CPPA_LOGF_DEBUG("handle event::write for " << io);
                    switch (io->continue_writing()) {
                        case read_failure:
                            io->io_failed(event::write);
                            // fall through
                        case write_closed:
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
                        case read_failure:
                            io->io_failed(event::read);
                            // fall through
                        case read_closed:
                            impl->stop_reader(io);
                            CPPA_LOGF_DEBUG("remove peer");
                            break;
                        default: break;
                    }
                    break;
                }
                case event::error: {
                    CPPA_LOGF_DEBUG("event::error; remove peer " << io);
                    io->io_failed(event::write);
                    io->io_failed(event::read);
                    impl->stop_reader(io);
                    impl->stop_writer(io);
                }
            }
        });
    }
    CPPA_LOGF_DEBUG("event loop done, erase all readers");
    // make sure to write everything before shutting down
    handler->for_each_reader([handler](continuable* ptr) {
        handler->erase_later(ptr, event::read);
    });
    handler->update();
    CPPA_LOGF_DEBUG("flush outgoing messages");
    CPPA_LOGF_DEBUG_IF(handler->num_sockets() == 0,
                       "nothing to flush, no writer left");
    while (handler->num_sockets() > 0) {
        handler->poll([&](event_bitmask mask, continuable* io) {
            switch (mask) {
                case event::write:
                    switch (io->continue_writing()) {
                        case write_failure:
                            io->io_failed(event::write);
                            // fall through
                        case write_closed:
                        case write_done:
                            handler->erase_later(io, event::write);
                            break;
                        default: break;
                    }
                    break;
                case event::error:
                    io->io_failed(event::write);
                    io->io_failed(event::read);
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
    CPPA_LOGF_DEBUG("middleman loop done");
}

} // namespace io

namespace {

std::atomic<size_t> default_max_msg_size{16 * 1024 * 1024};

} // namespace <anonymous>

void max_msg_size(size_t size)
{
  default_max_msg_size = size;
}

size_t max_msg_size()
{
  return default_max_msg_size;
}

} // namespace cppa
