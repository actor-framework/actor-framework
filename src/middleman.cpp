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


#include <tuple>
#include <cerrno>
#include <memory>
#include <cstring>
#include <sstream>
#include <iostream>

#include "cppa/on.hpp"
#include "cppa/actor.hpp"
#include "cppa/match.hpp"
#include "cppa/config.hpp"
#include "cppa/either.hpp"
#include "cppa/to_string.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/uniform_type_info.hpp"
#include "cppa/binary_deserializer.hpp"
#include "cppa/process_information.hpp"

#include "cppa/util/buffer.hpp"

#include "cppa/network/acceptor.hpp"
#include "cppa/network/middleman.hpp"
#include "cppa/network/input_stream.hpp"
#include "cppa/network/output_stream.hpp"
#include "cppa/network/addressed_message.hpp"
#include "cppa/network/default_peer_impl.hpp"
#include "cppa/network/default_peer_acceptor_impl.hpp"

#include "cppa/detail/fd_util.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"

//#define VERBOSE_MIDDLEMAN

#ifdef VERBOSE_MIDDLEMAN
#define DEBUG(arg) {                                                           \
    ostringstream oss;                                                         \
    oss << "[process id: "                                                     \
        << cppa::process_information::get()->process_id()                      \
        << "] " << arg << endl;                                                \
    cout << oss.str() << flush;                                                \
} (void) 0
#else
#define DEBUG(unused) ((void) 0)
#endif

//  use epoll event-loop implementation on Linux if not explicitly overriden
//  by using CPPA_POLL_IMPL, use (default) poll implementation otherwise
#if defined(CPPA_LINUX) && !defined(CPPA_POLL_IMPL)
#   define CPPA_EPOLL_IMPL
#   include <sys/epoll.h>
#else
#   ifndef CPPA_POLL_IMPL
#       define CPPA_POLL_IMPL
#   endif
#   include <poll.h>
#endif

using namespace std;

namespace cppa { namespace network {

namespace {

const size_t ui32_size = sizeof(uint32_t);

template<typename T, typename... Args>
void call_ctor(T& var, Args&&... args) {
    new (&var) T (forward<Args>(args)...);
}

template<typename T>
void call_dtor(T& var) {
    var.~T();
}

template<class Container, class Element>
void erase_from(Container& haystack, const Element& needle) {
    typedef typename Container::value_type value_type;
    auto last = end(haystack);
    auto i = find_if(begin(haystack), last, [&](const value_type& value) {
        return value == needle;
    });
    if (i != last) haystack.erase(i);
}

template<class Container, class UnaryPredicate>
void erase_from_if(Container& container, const UnaryPredicate& predicate) {
    auto last = end(container);
    auto i = find_if(begin(container), last, predicate);
    if (i != last) container.erase(i);
}

} // namespace <anonmyous>

enum class middleman_message_type {
    add_peer,
    publish,
    unpublish,
    outgoing_message,
    shutdown
};

struct middleman_message {
    middleman_message* next;
    const middleman_message_type type;
    union {
        std::pair<io_stream_ptr_pair,process_information_ptr> new_peer;
        std::pair<std::unique_ptr<acceptor>, actor_ptr> new_published_actor;
        actor_ptr published_actor;
        std::pair<process_information_ptr,addressed_message> out_msg;
    };

    middleman_message() : next(0), type(middleman_message_type::shutdown) { }

    middleman_message(io_stream_ptr_pair a0, process_information_ptr a1)
    : next(0), type(middleman_message_type::add_peer) {
        call_ctor(new_peer, move(a0), move(a1));
    }

    middleman_message(unique_ptr<acceptor> a0, actor_ptr a1)
    : next(0), type(middleman_message_type::publish) {
        call_ctor(new_published_actor, move(a0), move(a1));
    }

    middleman_message(actor_ptr a0)
    : next(0), type(middleman_message_type::unpublish) {
        call_ctor(published_actor, move(a0));
    }

    middleman_message(process_information_ptr a0, addressed_message a1)
    : next(0), type(middleman_message_type::outgoing_message) {
        call_ctor(out_msg, move(a0), move(a1));
    }

    ~middleman_message() {
        switch (type) {
            case middleman_message_type::add_peer:
                call_dtor(new_peer);
                break;
            case middleman_message_type::publish:
                call_dtor(new_published_actor);
                break;
            case middleman_message_type::unpublish:
                call_dtor(published_actor);
                break;
            case middleman_message_type::outgoing_message:
                call_dtor(out_msg);
                break;
            default: break;
        }
    }

    template<typename... Args>
    static inline std::unique_ptr<middleman_message> create(Args&&... args) {
        return std::unique_ptr<middleman_message>(new middleman_message(std::forward<Args>(args)...));
    }
};

typedef intrusive::single_reader_queue<middleman_message> middleman_queue;

class middleman_impl : public middleman {

    friend void middleman_loop(middleman_impl*);

 public:

    middleman_impl() { }

    void publish(std::unique_ptr<acceptor> server, const actor_ptr& aptr) {
        enqueue_message(middleman_message::create(move(server), aptr));
    }

    void add_peer(const io_stream_ptr_pair& io,
                  const process_information_ptr& node_info) {
        enqueue_message(middleman_message::create(io, node_info));
    }

    void unpublish(const actor_ptr& whom) {
        enqueue_message(middleman_message::create(whom));
    }

    void enqueue(const process_information_ptr& node,
                 const addressed_message& msg) {
        enqueue_message(middleman_message::create(node, msg));
    }

 protected:

    void start() {
        int pipefds[2];
        if (pipe(pipefds) != 0) { CPPA_CRITICAL("cannot create pipe"); }
        m_pipe_read = pipefds[0];
        m_pipe_write = pipefds[1];
        detail::fd_util::nonblocking(m_pipe_read, true);
        // start threads
        m_thread = std::thread([this] { middleman_loop(this); });
    }

    void stop() {
        enqueue_message(middleman_message::create());
        m_thread.join();
        close(m_pipe_read);
        close(m_pipe_write);
    }

 private:

    std::thread m_thread;
    native_socket_type m_pipe_read;
    native_socket_type m_pipe_write;
    middleman_queue m_queue;

    void enqueue_message(std::unique_ptr<middleman_message> msg) {
        m_queue._push_back(msg.release());
        std::atomic_thread_fence(std::memory_order_seq_cst);
        std::uint8_t dummy = 0;
        if (write(m_pipe_write, &dummy, sizeof(dummy)) != sizeof(dummy)) {
            CPPA_CRITICAL("cannot write to pipe");
        }
    }

};

middleman* middleman::create_singleton() {
    return new middleman_impl;
}

class middleman_overseer : public continuable_reader {

    typedef continuable_reader super;

 public:

    middleman_overseer(middleman* parent, int pipe_fd, middleman_queue& q)
    : super(parent, pipe_fd, false), m_queue(q) { }

    continue_reading_result continue_reading() {
        //DEBUG("middleman_overseer::continue_reading");
        static constexpr size_t num_dummies = 256;
        uint8_t dummies[num_dummies];
        auto read_result = ::read(read_handle(), dummies, num_dummies);
        if (read_result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // try again later
                return read_continue_later;
            }
            else {
                CPPA_CRITICAL("cannot read from pipe");
            }
        }
        atomic_thread_fence(memory_order_seq_cst);
        for (int i = 0; i < read_result; ++i) {
            unique_ptr<middleman_message> msg(m_queue.try_pop());
            if (!msg) { CPPA_CRITICAL("nullptr dequeued"); }
            switch (msg->type) {
                case middleman_message_type::add_peer: {
                    DEBUG("middleman_overseer: add_peer: "
                          << to_string(*(msg->new_peer.second)));
                    auto& new_peer = msg->new_peer;
                    auto& io_ptrs = new_peer.first;
                    peer_ptr p(new default_peer_impl(parent(),
                                                     io_ptrs.first,
                                                     io_ptrs.second,
                                                     new_peer.second));
                    parent()->add(p);
                    parent()->register_peer(*new_peer.second, p);
                    break;
                }
                case middleman_message_type::publish: {
                    DEBUG("middleman_overseer: publish");
                    auto& ptrs = msg->new_published_actor;
                    parent()->add(new default_peer_acceptor_impl(parent(),
                                                                 move(ptrs.first),
                                                                 ptrs.second));
                    break;
                }
                case middleman_message_type::unpublish: {
                    if (msg->published_actor) {
                        //DEBUG("middleman_overseer: unpublish actor id "
                        //      << msg->published_actor->id());
                        auto channel = parent()->acceptor_of(msg->published_actor);
                        if (channel) parent()->erase(channel);
                    }
                    break;
                }
                case middleman_message_type::outgoing_message: {
                    //DEBUG("middleman_overseer: outgoing_message");
                    auto& target_peer = msg->out_msg.first;
                    auto& out_msg = msg->out_msg.second;
                    CPPA_REQUIRE(target_peer != nullptr);
                    auto p = parent()->get_peer(*target_peer);
                    if (!p) { DEBUG("message to an unknown peer: "
                                    << to_string(out_msg)); }
                    else p->enqueue(out_msg);
                    break;
                }
                case middleman_message_type::shutdown: {
                    DEBUG("middleman: shutdown");
                    parent()->quit();
                    break;
                }
            }
        }
        return read_continue_later;
    }

 private:

    middleman_queue& m_queue;

};

typedef int event_bitmask;

namespace event {

static constexpr event_bitmask none  = 0x00;
static constexpr event_bitmask read  = 0x01;
static constexpr event_bitmask write = 0x02;
static constexpr event_bitmask both  = 0x03;
static constexpr event_bitmask error = 0x04;

} // namespace event

typedef std::tuple<native_socket_type,continuable_reader_ptr,event_bitmask>
        fd_meta_info;

class middleman_event_handler_base {

 public:

    virtual ~middleman_event_handler_base() { }

    void add_later(const continuable_reader_ptr& ptr, event_bitmask e) {
        append(m_additions, ptr, e);
    }

    void erase_later(const continuable_reader_ptr& ptr, event_bitmask e) {
        append(m_subtractions, ptr, e);
    }

 protected:

    vector<fd_meta_info> m_additions;
    vector<fd_meta_info> m_subtractions;

 private:

    void append(vector<fd_meta_info>& vec, const continuable_reader_ptr& ptr, event_bitmask e) {
        CPPA_REQUIRE(ptr != nullptr);
        CPPA_REQUIRE(e == event::read || e == event::write || e == event::both);
        if (e == event::read || (e == event::both && !ptr->is_peer())) {
            // ignore event::write unless ptr->is_peer
            vec.emplace_back(ptr->read_handle(), ptr, event::read);
        }
        else if (e == event::write) {
            CPPA_REQUIRE(ptr->is_peer());
            auto dptr = static_cast<peer*>(ptr.get());
            vec.emplace_back(dptr->write_handle(), ptr, event::write);
        }
        else { // e == event::both && ptr->is_peer()
            CPPA_REQUIRE(ptr->is_peer());
            CPPA_REQUIRE(e == event::both);
            auto dptr = static_cast<peer*>(ptr.get());
            auto rd = dptr->read_handle();
            auto wr = dptr->write_handle();
            if (rd == wr) vec.emplace_back(wr, ptr, event::both);
            else {
                vec.emplace_back(wr, ptr, event::write);
                vec.emplace_back(rd, ptr, event::read);
            }
        }
    }

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

    inline continue_reading_result continue_reading() {
        return ptr()->continue_reading();
    }

    inline continue_writing_result continue_writing() {
        return static_cast<peer*>(ptr())->continue_writing();
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

#ifdef CPPA_POLL_IMPL

typedef vector<pollfd>       pollfd_vector;
typedef vector<fd_meta_info> fd_meta_vector;

typedef pair<pollfd_vector::iterator,fd_meta_vector::iterator> pfd_iterator;

#ifndef POLLRDHUP
#define POLLRDHUP POLLHUP
#endif

struct pfd_access {

    inline void advance(pfd_iterator& i) const {
        ++(i.first);
        ++(i.second);
    }

    inline event_bitmask type(const pfd_iterator& i) const {
        auto revents = i.first->revents;
        if (revents == 0) return event::none;
        if (revents & (POLLRDHUP | POLLERR | POLLHUP | POLLNVAL)) {
            return event::error;
        }
        event_bitmask result = 0;
        if (revents & (POLLIN | POLLPRI)) result |= event::read;
        if (revents & POLLOUT) result |= event::write;
        CPPA_REQUIRE(result != 0);
        return result;
    }

    inline continuable_reader* ptr(pfd_iterator& i) const {
        return std::get<1>(*(i.second)).get();
    }

    inline bool equal(const pfd_iterator& lhs, const pfd_iterator& rhs) const {
        return lhs.first == rhs.first;
    }

    inline void handled(pfd_iterator& i) const { i.first->revents = 0; }

};

typedef event_iterator_impl<pfd_iterator,pfd_access> event_iterator;

class middleman_event_handler : public middleman_event_handler_base {

 public:

    inline void init() { }

    size_t num_sockets() const { return m_pollset.size(); }

    pair<event_iterator,event_iterator> poll() {
        CPPA_REQUIRE(m_pollset.empty() == false);
        CPPA_REQUIRE(m_pollset.size() == m_meta.size());
        for (;;) {
            auto presult = ::poll(m_pollset.data(), m_pollset.size(), -1);
            DEBUG("poll() on " << m_pollset.size() << " sockets returned " << presult);
            if (presult < 0) {
                switch (errno) {
                    // a signal was caught
                    case EINTR: {
                        // just try again
                        break;
                    }
                    case ENOMEM: {
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
            else return {event_iterator({begin(m_pollset), begin(m_meta)}),
                         event_iterator({end(m_pollset), end(m_meta)})};
        }
    }

    void update() {
        auto set_pollfd_events = [](pollfd& pfd, event_bitmask mask) {
            switch (mask) {
                case event::read:  pfd.events = POLLIN;           break;
                case event::write: pfd.events = POLLOUT;          break;
                case event::both:  pfd.events = (POLLIN|POLLOUT); break;
                default: CPPA_CRITICAL("invalid event bitmask");
            }
        };
        // first add, then erase (erase has higher priority)
        for (auto& add : m_additions) {
            CPPA_REQUIRE((std::get<2>(add) & event::both) != event::none);
            auto i = find_if(begin(m_meta), end(m_meta), [&](const fd_meta_info& other) {
                return std::get<0>(add) == std::get<0>(other);
            });
            pollfd* pfd;
            event_bitmask mask = std::get<2>(add);
            if (i != end(m_meta)) {
                CPPA_REQUIRE(std::get<1>(*i) == std::get<1>(add));
                mask |= std::get<2>(*i);
                pfd = &(m_pollset[distance(begin(m_meta), i)]);
            }
            else {
                pollfd tmp;
                tmp.fd = std::get<0>(add);
                tmp.revents = 0;
                tmp.events = 0;
                m_pollset.push_back(tmp);
                m_meta.push_back(add);
                pfd = &(m_pollset.back());
            }
            set_pollfd_events(*pfd, mask);
        }
        m_additions.clear();
        for (auto& sub : m_subtractions) {
            CPPA_REQUIRE((std::get<2>(sub) & event::both) != event::none);
            auto i = find_if(begin(m_meta), end(m_meta), [&](const fd_meta_info& other) {
                return std::get<0>(sub) == std::get<0>(other);
            });
            if (i != end(m_meta)) {
                auto pi = m_pollset.begin();
                advance(pi, distance(begin(m_meta), i));
                auto mask = std::get<2>(*i) & ~(std::get<2>(sub));
                switch (mask) {
                    case event::none: {
                        m_meta.erase(i);
                        m_pollset.erase(pi);
                        break;
                    }
                    default: set_pollfd_events(*pi, mask);
                }
            }
        }
        m_subtractions.clear();
    }

 private:

                              // _                                           ***
    pollfd_vector  m_pollset; //  \                                           **
                              //   > these two vectors are always in sync      *
    fd_meta_vector m_meta;    // _/                                           **
                              //                                             ***

};

#elif defined(CPPA_EPOLL_IMPL)

struct epoll_iterator_access {

    typedef vector<epoll_event>::iterator iterator;

    inline void advance(iterator& i) const { ++i; }

    inline event_bitmask type(const iterator& i) const {
        auto events = i->events;
        if (events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) return event::error;
        auto result = event::none;
        if (events & EPOLLIN)  result |= event::read;
        if (events & EPOLLOUT) result |= event::write;
        CPPA_REQUIRE(result != 0);
        return result;
    }

    inline continuable_reader* ptr(iterator& i) const {
        return reinterpret_cast<continuable_reader*>(i->data.ptr);
    }

    inline bool equal(const iterator& lhs, const iterator& rhs) const {
        return lhs == rhs;
    }

    inline void handled(iterator&) const { }

};

typedef event_iterator_impl<vector<epoll_event>::iterator,epoll_iterator_access>
        event_iterator;

class middleman_event_handler : public middleman_event_handler_base {

 public:

    middleman_event_handler() : m_epollfd(-1) { }

    ~middleman_event_handler() { if (m_epollfd != -1) close(m_epollfd); }

    void init() {
        m_epollfd = epoll_create1(EPOLL_CLOEXEC);
        if (m_epollfd == -1) throw ios_base::failure(  string("epoll_create1: ")
                                                     + strerror(errno));
        // handle at most 64 events at a time
        m_events.resize(64);
    }

    size_t num_sockets() const { return m_epoll_data.size(); }

    pair<event_iterator,event_iterator> poll() {
        CPPA_REQUIRE(m_epoll_data.empty() == false);
        for (;;) {
            DEBUG("epoll_wait on " << m_epoll_data.size() << " sockets");
            auto presult = epoll_wait(m_epollfd, m_events.data(),
                                      (int) m_events.size(), -1);
            DEBUG("epoll_wait returned " << presult);
            if (presult < 0) {
                // try again unless critical error occured
                presult = 0;
                switch (errno) {
                    // a signal was caught
                    case EINTR: {
                        // just try again
                        break;
                    }
                    default: {
                        perror("epoll() failed");
                        CPPA_CRITICAL("epoll() failed");
                    }
                }
            }
            else {
                auto first = begin(m_events);
                auto last = first;
                advance(last, presult);
                return {first, last};
            }
        }
    }

    void update() {
        handle_vec(m_additions, EPOLL_CTL_ADD);
        handle_vec(m_subtractions, EPOLL_CTL_DEL);
    }

 private:

    void handle_vec(vector<fd_meta_info>& vec, int eop) {
        for (auto& element : vec) {
            auto mask = std::get<2>(element);
            CPPA_REQUIRE(mask == event::write || mask == event::read || mask == event::both);
            auto ptr = std::get<1>(element).get();
            switch (mask) {
                case event::read:
                    epoll_op(eop, ptr->read_handle(), EPOLLIN, ptr);
                    break;
                case event::write: {
                    CPPA_REQUIRE(ptr->is_peer());
                    auto dptr = static_cast<peer*>(ptr);
                    epoll_op(eop, dptr->write_handle(), EPOLLOUT, dptr);
                    break;
                }
                case event::both: {
                    if (ptr->is_peer()) {
                        auto dptr = static_cast<peer*>(ptr);
                        auto rd = dptr->read_handle();
                        auto wr = dptr->write_handle();
                        if (rd == wr) epoll_op(eop, wr, EPOLLIN|EPOLLOUT, dptr);
                        else {
                            epoll_op(eop, rd, EPOLLIN, ptr);
                            epoll_op(eop, wr, EPOLLOUT, dptr);
                        }
                    }
                    else epoll_op(eop, ptr->read_handle(), EPOLLIN, ptr);
                    break;
                }
                default: CPPA_CRITICAL("invalid event mask found in handle_vec");
            }
        }
        vec.clear();
    }

    // operation: EPOLL_CTL_ADD or EPOLL_CTL_DEL
    // mask: EPOLLIN, EPOLLOUT or (EPOLLIN | EPOLLOUT)
    void epoll_op(int operation, int fd, uint32_t mask, continuable_reader* ptr) {
        CPPA_REQUIRE(ptr != nullptr);
        CPPA_REQUIRE(operation == EPOLL_CTL_ADD || operation == EPOLL_CTL_DEL);
        CPPA_REQUIRE(mask == EPOLLIN || mask == EPOLLOUT || mask == (EPOLLIN|EPOLLOUT));
        epoll_event ee;
        // also fire event on peer shutdown for input operations
        ee.events = (mask & EPOLLIN) ? (mask | EPOLLRDHUP) : mask;
        ee.data.ptr = ptr;
        // check wheter fd is already registered to epoll
        auto iter = m_epoll_data.find(fd);
        if (iter != end(m_epoll_data)) {
            CPPA_REQUIRE(ee.data.ptr == iter->second.data.ptr);
            if (operation == EPOLL_CTL_ADD) {
                if (mask == iter->second.events) {
                    // nothing to do here, fd is already registered with
                    // correct bitmask
                    return;
                }
                // modify instead
                operation = EPOLL_CTL_MOD;
                ee.events |= iter->second.events; // new bitmask
                iter->second.events = ee.events;  // update bitmask in map
            }
            else if (operation == EPOLL_CTL_DEL) {
                // check wheter we have fd registered for other events
                ee.events = iter->second.events & ~(ee.events);
                if (ee.events != 0) {
                    iter->second.events = ee.events; // update bitmask in map
                    operation = EPOLL_CTL_MOD;       // modify instead
                }
                else m_epoll_data.erase(iter); // erase from map as well
            }
        }
        else if (operation == EPOLL_CTL_DEL) {
            // nothing to delete here
            return;
        }
        else { // operation == EPOLL_CTL_ADD
            CPPA_REQUIRE(operation == EPOLL_CTL_ADD);
            m_epoll_data.insert(make_pair(fd, ee));
        }
        if (epoll_ctl(m_epollfd, operation, fd, &ee) < 0) {
            switch (errno) {
                // supplied file descriptor is already registered
                case EEXIST: {
                    // shouldn't happen, but no big deal
                    cerr << "*** warning: file descriptor registered twice\n"
                         << flush;
                    break;
                }
                // op was EPOLL_CTL_MOD or EPOLL_CTL_DEL,
                // and fd is not registered with this epoll instance.
                case ENOENT: {
                    cerr << "*** warning: cannot delete file descriptor "
                            "because it isn't registered\n"
                         << flush;
                    break;
                }
                default: {
                    perror("epoll_ctl() failed");
                    CPPA_CRITICAL("epoll_ctl() failed");
                }
            }
        }
    }

    int m_epollfd;
    vector<epoll_event> m_events;
    map<native_socket_type,epoll_event> m_epoll_data;

};

#endif

middleman::middleman() : m_done(false), m_handler(new middleman_event_handler){}

middleman::~middleman() { }

void middleman::register_peer(const process_information& node,
                              const peer_ptr& ptr) {
    auto& ptrref = m_peers[node];
    if (ptrref) { DEBUG("peer already defined!"); }
    else ptrref = ptr;
}

void middleman::continue_writing_later(const peer_ptr& ptr) {
    m_handler->add_later(ptr, event::write);
}

void middleman::add(const continuable_reader_ptr& what) {
    m_readers.push_back(what);
    m_handler->add_later(what, event::read);
}

void middleman::erase(const continuable_reader_ptr& what) {
    m_handler->erase_later(what, event::both);
    erase_from(m_readers, what);
    erase_from_if(m_peers, [=](const map<process_information,peer_ptr>::value_type& kvp) {
        return kvp.second == what;
    });
}

continuable_reader_ptr middleman::acceptor_of(const actor_ptr& whom) {
    auto e = end(m_readers);
    auto i = find_if(begin(m_readers), e, [=](const continuable_reader_ptr& crp) {
        return crp->is_acceptor_of(whom);
    });
    return (i != e) ? *i : nullptr;
}

peer_ptr middleman::get_peer(const process_information& node) {
    auto e = end(m_peers);
    auto i = m_peers.find(node);
    return (i != e) ? i->second : nullptr;
}

void middleman_loop(middleman_impl* impl) {
    middleman_event_handler* handler = impl->m_handler.get();
    DEBUG("run middleman loop");
    handler->init();
    impl->add(new middleman_overseer(impl, impl->m_pipe_read, impl->m_queue));
    handler->update();
    while (!impl->done()) {
        auto iters = handler->poll();
        for (auto i = iters.first; i != iters.second; ++i) {
            auto mask = i->type();
            switch (mask) {
                default: CPPA_CRITICAL("invalid event");
                case event::none: break;
                case event::both:
                case event::write: {
                    DEBUG("handle event::write");
                    switch (i->continue_writing()) {
                        case write_closed:
                        case write_failure:
                            impl->erase(i->ptr());
                            break;
                        case write_done:
                            handler->erase_later(i->ptr(), event::write);
                            break;
                        default: break;
                    }
                    if (mask == event::write) break;
                    // else: fall through
                    DEBUG("handle event::both; fall through");
                }
                case event::read: {
                    DEBUG("handle event::read");
                    switch (i->continue_reading()) {
                        case read_closed:
                        case read_failure:
                            impl->erase(i->ptr());
                            break;
                        default: break;
                    }
                    break;
                }
                case event::error: {
                    impl->erase(i->ptr());
                    // calls handler->erase_later(i->ptr(), event::both);
                }
            }
            i->handled();
        }
        handler->update();
    }
    DEBUG("flush outgoing messages");
    // make sure to write everything before shutting down
    for (auto ptr : impl->m_readers) { handler->erase_later(ptr, event::read); }
    handler->update();
    while (handler->num_sockets() > 0) {
        auto iters = handler->poll();
        for (auto i = iters.first; i != iters.second; ++i) {
            switch (i->type()) {
                case event::write:
                    switch (i->continue_writing()) {
                        case write_closed:
                        case write_failure:
                        case write_done:
                            handler->erase_later(i->ptr(), event::write);
                            break;
                        default: break;
                    }
                    break;
                default: CPPA_CRITICAL("expected event::write only "
                                       "during shutdown phase");
            }
        }
        handler->update();
    }
    DEBUG("middleman loop done");
}

} } // namespace cppa::detail
