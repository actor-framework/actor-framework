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
#include "cppa/node_id.hpp"
#include "cppa/to_string.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/uniform_type_info.hpp"
#include "cppa/thread_mapped_actor.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/util/buffer.hpp"

#include "cppa/io/peer.hpp"
#include "cppa/io/acceptor.hpp"
#include "cppa/io/middleman.hpp"
#include "cppa/io/input_stream.hpp"
#include "cppa/io/output_stream.hpp"
#include "cppa/io/peer_acceptor.hpp"
#include "cppa/io/remote_actor_proxy.hpp"
#include "cppa/io/default_message_queue.hpp"
#include "cppa/io/middleman_event_handler.hpp"

#include "cppa/detail/fd_util.hpp"
#include "cppa/detail/actor_registry.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

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

void middleman::continue_writer(continuable* ptr) {
    CPPA_LOG_TRACE(CPPA_ARG(ptr));
    m_handler->add_later(ptr, event::write);
}
    
void middleman::stop_writer(continuable* ptr) {
    CPPA_LOG_TRACE(CPPA_ARG(ptr));
    m_handler->erase_later(ptr, event::write);
}
    
bool middleman::has_writer(continuable* ptr) {
    return m_handler->has_writer(ptr);
}
    
void middleman::continue_reader(continuable* ptr) {
    CPPA_LOG_TRACE(CPPA_ARG(ptr));
    m_handler->add_later(ptr, event::read);
}
    
void middleman::stop_reader(continuable* ptr) {
    CPPA_LOG_TRACE(CPPA_ARG(ptr));
    m_handler->erase_later(ptr, event::read);
}
    
bool middleman::has_reader(continuable* ptr) {
    return m_handler->has_reader(ptr);
}
    
typedef intrusive::single_reader_queue<middleman_event> middleman_queue;

/*
 * A middleman also implements a "namespace" for actors.
 */
class middleman_impl : public middleman {

    friend class middleman;
    friend void middleman_loop(middleman_impl*);

 public:

    middleman_impl() : m_done(false) {
        m_handler = middleman_event_handler::create();
        m_namespace.set_proxy_factory([=](actor_id aid, node_id_ptr ptr) {
            return make_counted<remote_actor_proxy>(aid, std::move(ptr), this);
        });
        m_namespace.set_new_element_callback([=](actor_id aid, const node_id& node) {
            deliver(node,
                    {invalid_actor_addr, nullptr},
                    make_any_tuple(atom("MONITOR"),
                                   node_id::get(),
                                   aid));
        });
    }

    void run_later(function<void()> fun) override {
        m_queue.enqueue(new middleman_event(move(fun)));
        atomic_thread_fence(memory_order_seq_cst);
        uint8_t dummy = 0;
        // ignore result; write error only means middleman already exited
        static_cast<void>(::write(m_pipe_write, &dummy, sizeof(dummy)));
    }

    void register_peer(const node_id& node, peer* ptr) override {
        CPPA_LOG_TRACE("node = " << to_string(node) << ", ptr = " << ptr);
        auto& entry = m_peers[node];
        if (entry.impl == nullptr) {
            if (entry.queue == nullptr) entry.queue.emplace();
            ptr->set_queue(entry.queue);
            entry.impl = ptr;
            if (!entry.queue->empty()) {
                auto tmp = entry.queue->pop();
                ptr->enqueue(tmp.first, tmp.second);
            }
            CPPA_LOG_INFO("peer " << to_string(node) << " added");
        }
        else {
            CPPA_LOG_WARNING("peer " << to_string(node) << " already defined, "
                             "multiple calls to remote_actor()?");
        }
    }
    
    peer* get_peer(const node_id& node) override {
        CPPA_LOG_TRACE("n = " << to_string(n));
        auto i = m_peers.find(node);
        if (i != m_peers.end()) {
            CPPA_LOG_DEBUG("result = " << i->second.impl);
            return i->second.impl;
        }
        CPPA_LOGMF(CPPA_DEBUG, self, "result = nullptr");
        return nullptr;
    }
    
    void del_acceptor(peer_acceptor* ptr) override {
        auto i = m_acceptors.begin();
        auto e = m_acceptors.end();
        while (i != e) {
            auto& vec = i->second;
            auto last = vec.end();
            auto iter = std::find(vec.begin(), last, ptr);
            if (iter != last) vec.erase(iter);
            if (not vec.empty()) ++i;
            else i = m_acceptors.erase(i);
        }
    }
    
    void deliver(const node_id& node,
                 const message_header& hdr,
                 any_tuple msg                  ) override {
        auto& entry = m_peers[node];
        if (entry.impl) {
            CPPA_REQUIRE(entry.queue != nullptr);
            if (!entry.impl->has_unwritten_data()) {
                CPPA_REQUIRE(entry.queue->empty());
                entry.impl->enqueue(hdr, msg);
                return;
            }
        }
        if (entry.queue == nullptr) entry.queue.emplace();
        entry.queue->emplace(hdr, msg);
    }

    void last_proxy_exited(peer* pptr) override {
        CPPA_REQUIRE(pptr != nullptr);
        CPPA_LOG_TRACE(CPPA_ARG(pptr)
                       << ", pptr->node() = " << to_string(pptr->node()));
        if (pptr->erase_on_last_proxy_exited() && pptr->queue().empty()) {
            stop_reader(pptr);
            auto i = m_peers.find(pptr->node());
            if (i != m_peers.end()) {
                CPPA_LOG_DEBUG_IF(i->second.impl != pptr,
                                  "node " << to_string(pptr->node())
                                  << " does not exist in m_peers");
                if (i->second.impl == pptr) {
                    m_peers.erase(i);
                }
            }
        }
    }
    

    void new_peer(const input_stream_ptr& in,
                  const output_stream_ptr& out,
                  const node_id_ptr& node = nullptr) override {
        CPPA_LOG_TRACE("");
        auto ptr = new peer(this, in, out, node);
        continue_reader(ptr);
        if (node) register_peer(*node, ptr);
    }
    
    void register_acceptor(const actor_addr& whom, peer_acceptor* ptr) override {
        run_later([=] {
            CPPA_LOGC_TRACE("cppa::io::middleman",
                            "register_acceptor$lambda", "");
            m_acceptors[whom].push_back(ptr);
            continue_reader(ptr);
        });
    }
    
 protected:

    void initialize() override {
        int pipefds[2];
        if (pipe(pipefds) != 0) { CPPA_CRITICAL("cannot create pipe"); }
        m_pipe_read = pipefds[0];
        m_pipe_write = pipefds[1];
        detail::fd_util::nonblocking(m_pipe_read, true);
        // start threads
        m_thread = thread([this] { middleman_loop(this); });
    }

    void destroy() override {
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

    middleman_event_handler& handler();

    thread m_thread;
    native_socket_type m_pipe_read;
    native_socket_type m_pipe_write;
    middleman_queue m_queue;

    struct peer_entry {
        peer* impl;
        default_message_queue_ptr queue;
    };
    
    std::map<actor_addr, std::vector<peer_acceptor*>> m_acceptors;
    std::map<node_id, peer_entry> m_peers;

};

class middleman_overseer : public continuable {

    typedef continuable super;

 public:

    middleman_overseer(int pipe_fd, middleman_queue& q)
    : super(pipe_fd), m_queue(q) { }

    void dispose() override {
        delete this;
    }

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

void middleman_loop(middleman_impl* impl) {
#   ifdef CPPA_LOG_LEVEL
    auto mself = make_counted<thread_mapped_actor>();
    CPPA_SET_DEBUG_NAME("middleman");
#   endif
    middleman_event_handler* handler = impl->m_handler.get();
    CPPA_LOGF_TRACE("run middleman loop");
    CPPA_LOGF_INFO("middleman runs at "
                   << to_string(*node_id::get()));
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

middleman* middleman::create_singleton() {
    return new middleman_impl;
}

} } // namespace cppa::detail
