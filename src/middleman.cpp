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


#include <set>
#include <map>
#include <vector>
#include <cstring>
#include <sstream>
#include <iostream>

#include "cppa/on.hpp"
#include "cppa/actor.hpp"
#include "cppa/match.hpp"
#include "cppa/config.hpp"
#include "cppa/to_string.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/uniform_type_info.hpp"
#include "cppa/binary_deserializer.hpp"
#include "cppa/process_information.hpp"

#include "cppa/util/buffer.hpp"
#include "cppa/util/acceptor.hpp"
#include "cppa/util/input_stream.hpp"
#include "cppa/util/output_stream.hpp"

#include "cppa/detail/middleman.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/addressed_message.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"

using namespace std;

#define DEBUG(arg) {                                                           \
    ostringstream oss;                                                         \
    oss << "[process id: "                                                     \
        << cppa::process_information::get()->process_id()                      \
        << "] " << arg << endl;                                                \
    cout << oss.str();                                                         \
} (void) 0

#undef DEBUG
#define DEBUG(unused) ((void) 0)

namespace cppa { namespace detail {

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

middleman_message::middleman_message()
: next(0), type(middleman_message_type::shutdown) { }

middleman_message::middleman_message(util::io_stream_ptr_pair a0,
                                     process_information_ptr a1)
: next(0), type(middleman_message_type::add_peer) {
    call_ctor(new_peer, move(a0), move(a1));
}

middleman_message::middleman_message(unique_ptr<util::acceptor> a0,
                                     actor_ptr a1)
: next(0), type(middleman_message_type::publish) {
    call_ctor(new_published_actor, move(a0), move(a1));
}

middleman_message::middleman_message(actor_ptr a0)
: next(0), type(middleman_message_type::unpublish) {
    call_ctor(published_actor, move(a0));
}

middleman_message::middleman_message(process_information_ptr a0,
                                     addressed_message a1)
: next(0), type(middleman_message_type::outgoing_message) {
    call_ctor(out_msg, move(a0), move(a1));
}

middleman_message::~middleman_message() {
    switch (type) {
        case middleman_message_type::add_peer: {
            call_dtor(new_peer);
            break;
        }
        case middleman_message_type::publish: {
            call_dtor(new_published_actor);
            break;
        }
        case middleman_message_type::unpublish: {
            call_dtor(published_actor);
            break;
        }
        case middleman_message_type::outgoing_message: {
            call_dtor(out_msg);
            break;
        }
        default: break;
    }
}

class middleman;

typedef intrusive::single_reader_queue<middleman_message> middleman_queue;

class network_channel : public ref_counted {

 public:

    network_channel(middleman* ptr, native_socket_type read_fd)
    : m_parent(ptr), m_read_handle(read_fd) { }

    virtual bool continue_reading() = 0;

    inline native_socket_type read_handle() const {
        return m_read_handle;
    }

    virtual bool is_acceptor_of(const actor_ptr&) const {
        return false;
    }

 protected:

    inline middleman* parent() { return m_parent; }
    inline const middleman* parent() const { return m_parent; }

 private:

    middleman* m_parent;
    native_socket_type m_read_handle;

};

typedef intrusive_ptr<network_channel> network_channel_ptr;
typedef vector<network_channel_ptr> network_channel_ptr_vector;

class peer_connection : public network_channel {

    typedef network_channel super;

 public:

    peer_connection(middleman* parent,
                    util::input_stream_ptr istream,
                    util::output_stream_ptr ostream,
                    process_information_ptr peer_ptr = nullptr)
    : super(parent, istream->read_file_handle())
    , m_istream(istream), m_ostream(ostream), m_peer(peer_ptr)
    , m_rd_state((peer_ptr) ? wait_for_msg_size : wait_for_process_info)
    , m_meta_msg(uniform_typeid<addressed_message>())
    , m_has_unwritten_data(false)
    , m_write_handle(ostream->write_file_handle()) {
        m_rd_buf.reset(m_rd_state == wait_for_process_info
                       ? ui32_size + process_information::node_id_size
                       : ui32_size);
    }

    ~peer_connection() {
        if (m_peer) {
            // collect all children (proxies to actors of m_peer)
            vector<actor_proxy_ptr> children;
            children.reserve(20);
            get_actor_proxy_cache().erase_all(m_peer->node_id(),
                                              m_peer->process_id(),
                                              [&](actor_proxy_ptr& pptr) {
                children.push_back(move(pptr));
            });
            // kill all proxies
            for (actor_proxy_ptr& pptr: children) {
                pptr->enqueue(nullptr,
                              make_any_tuple(atom("KILL_PROXY"),
                                             exit_reason::remote_link_unreachable));
            }
        }
    }

    inline native_socket_type write_handle() const {
        return m_write_handle;
    }

    bool continue_reading();

    bool continue_writing() {
        DEBUG("peer_connection::continue_writing");
        if (has_unwritten_data()) {
            size_t written;
            written = m_ostream->write_some(m_wr_buf.data(),
                                            m_wr_buf.size());
            if (written != m_wr_buf.size()) {
                m_wr_buf.erase_leading(written);
            }
            else {
                m_wr_buf.reset();
                has_unwritten_data(false);
            }
        }
        return true;
    }

    void write(const addressed_message& msg) {
        binary_serializer bs(&m_wr_buf);
        bs << msg;
        if (!has_unwritten_data()) {
            size_t written = m_ostream->write_some(m_wr_buf.data(),
                                                   m_wr_buf.size());
            if (written != m_wr_buf.size()) {
                m_wr_buf.erase_leading(written);
                has_unwritten_data(true);
            }
            else {
                m_wr_buf.reset();
            }
        }
    }

    inline bool has_unwritten_data() const {
        return m_has_unwritten_data;
    }

 protected:

    inline void has_unwritten_data(bool value) {
        m_has_unwritten_data = value;
    }

 private:

    enum read_state {
        // connection just established; waiting for process information
        wait_for_process_info,
        // wait for the size of the next message
        wait_for_msg_size,
        // currently reading a message
        read_message
    };

    util::input_stream_ptr m_istream;
    util::output_stream_ptr m_ostream;
    process_information_ptr m_peer;
    read_state m_rd_state;
    const uniform_type_info* m_meta_msg;
    bool m_has_unwritten_data;
    native_socket_type m_write_handle;

    util::buffer m_rd_buf;
    util::buffer m_wr_buf;

};

typedef intrusive_ptr<peer_connection> peer_connection_ptr;
typedef map<process_information, peer_connection_ptr> peer_map;

class middleman {

 public:

    middleman() : m_done(false), m_pself(process_information::get()) { }

    template<class Connection, typename... Args>
    inline void add_channel(Args&&... args) {
        m_new_channels.emplace_back(new Connection(this, forward<Args>(args)...));
    }

    inline void add_channel_ptr(network_channel_ptr ptr) {
        m_new_channels.push_back(move(ptr));
    }

    inline void add_peer(const process_information& pinf, peer_connection_ptr cptr) {
        auto& ptrref = m_peers[pinf];
        if (ptrref) {
            DEBUG("peer already defined!");
        }
        else {
            ptrref = cptr;
        }
    }

    void operator()(int pipe_fd, middleman_queue& queue);

    inline const process_information_ptr& pself() {
        return m_pself;
    }

    inline void quit() {
        m_done = true;
    }

    peer_connection_ptr peer(const process_information& pinf) {
        auto i = m_peers.find(pinf);
        if (i != m_peers.end()) {
            CPPA_REQUIRE(i->second != nullptr);
            return i->second;
        }
        return nullptr;
    }

    network_channel_ptr acceptor_of(const actor_ptr& whom) {
        auto last = m_channels.end();
        auto i = find_if(m_channels.begin(), last, [=](network_channel_ptr& ptr) {
            return ptr->is_acceptor_of(whom);
        });
        return (i != last) ? *i : nullptr;
    }

    void continue_writing(peer_connection_ptr ptr) {
        m_peers_with_unwritten_data.insert(move(ptr));
    }

    void erase(network_channel_ptr ptr) {
        m_erased_channels.insert(move(ptr));
    }

 private:

    bool m_done;
    process_information_ptr m_pself;
    peer_map m_peers;
    network_channel_ptr_vector m_channels;
    network_channel_ptr_vector m_new_channels;
    set<peer_connection_ptr> m_peers_with_unwritten_data;

    set<network_channel_ptr> m_erased_channels;

};

bool peer_connection::continue_reading() {
    //DEBUG("peer_connection::continue_reading");
    for (;;) {
        m_rd_buf.append_from(m_istream.get());
        if (!m_rd_buf.full()) return true; // try again later
        switch (m_rd_state) {
            case wait_for_process_info: {
                //DEBUG("peer_connection::continue_reading: "
                //      "wait_for_process_info");
                uint32_t process_id;
                process_information::node_id_type node_id;
                memcpy(&process_id, m_rd_buf.data(), sizeof(uint32_t));
                memcpy(node_id.data(), m_rd_buf.data() + sizeof(uint32_t),
                       process_information::node_id_size);
                m_peer.reset(new process_information(process_id, node_id));
                parent()->add_peer(*m_peer, this);
                // initialization done
                m_rd_state = wait_for_msg_size;
                m_rd_buf.reset(sizeof(uint32_t));
                DEBUG("pinfo read: "
                      << m_peer->process_id()
                      << "@"
                      << to_string(m_peer->node_id()));
                break;
            }
            case wait_for_msg_size: {
                //DEBUG("peer_connection::continue_reading: wait_for_msg_size");
                uint32_t msg_size;
                memcpy(&msg_size, m_rd_buf.data(), sizeof(uint32_t));
                //DEBUG("msg_size: " << msg_size);
                m_rd_buf.reset(msg_size);
                m_rd_state = read_message;
                break;
            }
            case read_message: {
                //DEBUG("peer_connection::continue_reading: read_message");
                addressed_message msg;
                binary_deserializer bd(m_rd_buf.data(), m_rd_buf.size());
                m_meta_msg->deserialize(&msg, &bd);
                auto& content = msg.content();
                DEBUG("<-- " << to_string(msg));
                match(content) (
                    // monitor messages are sent automatically whenever
                    // actor_proxy_cache creates a new proxy
                    // note: aid is the *original* actor id
                    on(atom("MONITOR"), arg_match) >> [&](const process_information_ptr& peer, actor_id aid) {
                        if (!peer) {
                            DEBUG("MONITOR received from invalid peer");
                            return;
                        }
                        auto ar = singleton_manager::get_actor_registry();
                        auto reg_entry = ar->get_entry(aid);
                        auto pself = parent()->pself();
                        auto send_kp = [=](uint32_t reason) {
                            middleman_enqueue(peer,
                                              nullptr,
                                              nullptr,
                                              make_any_tuple(
                                                  atom("KILL_PROXY"),
                                                  pself,
                                                  aid,
                                                  reason
                                              ));
                        };
                        if (reg_entry.first == nullptr) {
                            if (reg_entry.second == exit_reason::not_exited) {
                                // invalid entry
                                DEBUG("MONITOR for an unknown actor received");
                            }
                            else {
                                // this actor already finished execution;
                                // reply with KILL_PROXY message
                                send_kp(reg_entry.second);
                            }
                        }
                        else {
                            reg_entry.first->attach_functor(send_kp);
                        }
                    },
                    on(atom("KILL_PROXY"), arg_match) >> [&](const process_information_ptr& peer, actor_id aid, std::uint32_t reason) {
                        auto& cache = get_actor_proxy_cache();
                        auto proxy = cache.get(aid,
                                               peer->process_id(),
                                               peer->node_id());
                        if (proxy) {
                            proxy->enqueue(nullptr,
                                           make_any_tuple(
                                               atom("KILL_PROXY"), reason));
                        }
                        else {
                            DEBUG("received KILL_PROXY message but didn't "
                                  "found matching instance in cache");
                        }
                    },
                    on(atom("LINK"), arg_match) >> [&](const actor_ptr& ptr) {
                        if (msg.sender()->is_proxy() == false) {
                            DEBUG("msg.sender() is not a proxy");
                            return;
                        }
                        auto whom = msg.sender().downcast<actor_proxy>();
                        if ((whom) && (ptr)) whom->local_link_to(ptr);
                    },
                    on(atom("UNLINK"), arg_match) >> [](const actor_ptr& ptr) {
                        if (ptr->is_proxy() == false) {
                            DEBUG("msg.sender() is not a proxy");
                            return;
                        }
                        auto whom = ptr.downcast<actor_proxy>();
                        if ((whom) && (ptr)) whom->local_unlink_from(ptr);
                    },
                    others() >> [&] {
                        auto receiver = msg.receiver().get();
                        if (receiver) {
                            if (msg.id().valid()) {
                                auto ra = dynamic_cast<actor*>(receiver);
                                DEBUG("sync message for actor "
                                      << ra->id());
                                if (ra) {
                                    ra->sync_enqueue(
                                        msg.sender().get(),
                                        msg.id(),
                                        move(msg.content()));
                                }
                                else{
                                    DEBUG("ERROR: sync message to a non-actor");
                                }
                            }
                            else {
                                DEBUG("async message (sender is "
                                      << (msg.sender() ? "valid" : "NULL")
                                      << ")");
                                receiver->enqueue(
                                    msg.sender().get(),
                                    move(msg.content()));
                            }
                        }
                        else {
                            DEBUG("empty receiver");
                        }
                    }
                );
                m_rd_buf.reset(sizeof(uint32_t));
                m_rd_state = wait_for_msg_size;
                break;
            }
            default: {
                CPPA_CRITICAL("illegal state");
            }
        }
        // try to read more (next iteration)
    }
}

class peer_acceptor : public network_channel {

    typedef network_channel super;

 public:

    peer_acceptor(middleman* parent,
                  actor_id aid,
                  unique_ptr<util::acceptor> acceptor)
    : super(parent, acceptor->acceptor_file_handle())
    , m_actor_id(aid)
    , m_acceptor(move(acceptor)) { }

    bool is_doorman_of(actor_id aid) const {
        return m_actor_id == aid;
    }

    bool continue_reading() {
        //DEBUG("peer_acceptor::continue_reading");
        // accept as many connections as possible
        for (;;) {
            auto opt = m_acceptor->try_accept_connection();
            if (opt) {
                auto& pair = *opt;
                auto& pself = parent()->pself();
                uint32_t process_id = pself->process_id();
                pair.second->write(&m_actor_id, sizeof(actor_id));
                pair.second->write(&process_id, sizeof(uint32_t));
                pair.second->write(pself->node_id().data(),
                                   pself->node_id().size());
                parent()->add_channel<peer_connection>(pair.first,
                                                       pair.second);
            }
            else {
                return true;
            }
       }
    }

 private:

    actor_id m_actor_id;
    unique_ptr<util::acceptor> m_acceptor;

};

class middleman_overseer : public network_channel {

    typedef network_channel super;

 public:

    middleman_overseer(middleman* parent, int pipe_fd, middleman_queue& q)
    : super(parent, pipe_fd), m_queue(q) { }

    bool continue_reading() {
        //DEBUG("middleman_overseer::continue_reading");
        static constexpr size_t num_dummies = 256;
        uint8_t dummies[num_dummies];
        auto read_result = ::read(read_handle(), dummies, num_dummies);
        if (read_result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // try again later
                return true;
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
                    peer_connection_ptr peer;
                    peer.reset(new peer_connection(parent(),
                                                   io_ptrs.first,
                                                   io_ptrs.second,
                                                   new_peer.second));
                    parent()->add_channel_ptr(peer);
                    parent()->add_peer(*(new_peer.second), peer);
                    break;
                }
                case middleman_message_type::publish: {
                    DEBUG("middleman_overseer: publish");
                    auto& ptrs = msg->new_published_actor;
                    parent()->add_channel<peer_acceptor>(ptrs.second->id(),
                                                         move(ptrs.first));
                    break;
                }
                case middleman_message_type::unpublish: {
                    if (msg->published_actor) {
                        //DEBUG("middleman_overseer: unpublish actor id "
                        //      << msg->published_actor->id());
                        auto channel = parent()->acceptor_of(msg->published_actor);
                        if (channel) {
                            parent()->erase(channel);
                        }
                    }
                    break;
                }
                case middleman_message_type::outgoing_message: {
                    //DEBUG("middleman_overseer: outgoing_message");
                    auto& target_peer = msg->out_msg.first;
                    auto& out_msg = msg->out_msg.second;
                    CPPA_REQUIRE(target_peer != nullptr);
                    auto peer = parent()->peer(*target_peer);
                    if (!peer) {
                        DEBUG("message to an unknown peer: " << to_string(out_msg));
                        break;
                    }
                    DEBUG("--> " << to_string(out_msg));
                    auto had_unwritten_data = peer->has_unwritten_data();
                    try {
                        peer->write(out_msg);
                        if (!had_unwritten_data && peer->has_unwritten_data()) {
                            parent()->continue_writing(peer);
                        }
                    }
                    catch (exception& e) {
                        DEBUG("peer disconnected: " << e.what());
                        parent()->erase(peer);
                    }
                    break;
                }
                case middleman_message_type::shutdown: {
                    DEBUG("middleman: shutdown");
                    parent()->quit();
                    break;
                }
            }
        }
        return true;
    }

 private:

    middleman_queue& m_queue;

};

void middleman::operator()(int pipe_fd, middleman_queue& queue) {
    DEBUG("pself: " << to_string(*m_pself));
    int maxfd = 0;
    fd_set rdset;
    fd_set wrset;
    fd_set* wrset_ptr = nullptr;
    m_channels.emplace_back(new middleman_overseer(this, pipe_fd, queue));
    do {
        FD_ZERO(&rdset);
        maxfd = 0;
        CPPA_REQUIRE(m_channels.size() > 0);
        for (auto& channel : m_channels) {
            auto fd = channel->read_handle();
            maxfd = max(maxfd, fd);
            FD_SET(fd, &rdset);
        }
        if (m_peers_with_unwritten_data.empty()) {
            if (wrset_ptr) wrset_ptr = nullptr;
        }
        else {
            for (auto& peer : m_peers_with_unwritten_data) {
                auto fd = peer->write_handle();
                maxfd = max(maxfd, fd);
                FD_SET(fd, &wrset);
            }
            wrset_ptr = &wrset;
        }
        CPPA_REQUIRE(maxfd > 0);
        //DEBUG("select()");
        int sresult;
        do {
            sresult = select(maxfd + 1, &rdset, wrset_ptr, nullptr, nullptr);
            if (sresult < 0) {
                CPPA_CRITICAL("select() failed");
            }
        }
        while (sresult == 0);
        //DEBUG("continue reading ...");
        { // iterate over all channels and remove channels as needed
            for (auto& channel : m_channels) {
                if (FD_ISSET(channel->read_handle(), &rdset)) {
                    bool erase_channel = false;
                    try { erase_channel = !channel->continue_reading(); }
                    catch (exception& e) {
                        DEBUG(demangle(typeid(e).name()) << ": " << e.what());
                        erase_channel = true;
                    }
                    if (erase_channel) {
                        DEBUG("erase worker");
                        m_erased_channels.insert(channel);
                    }
                }
            }
        }
        if (wrset_ptr) { // iterate over peers with unwritten data
            DEBUG("continue writing ...");
            for (auto& peer : m_peers_with_unwritten_data) {
                if (FD_ISSET(peer->write_handle(), &wrset)) {
                    bool erase_channel = false;
                    try { erase_channel = !peer->continue_writing(); }
                    catch (exception& e) {
                        DEBUG(demangle(typeid(e).name()) << ": " << e.what());
                        erase_channel = true;
                    }
                    if (erase_channel) {
                        DEBUG("erase worker");
                        m_erased_channels.insert(peer);
                    }
                }
            }
        }
        // insert new handlers
        if (m_new_channels.empty() == false) {
            DEBUG("insert new channel(s)");
            move(m_new_channels.begin(), m_new_channels.end(),
                 back_inserter(m_channels));
            m_new_channels.clear();
        }
        if (!m_erased_channels.empty()) {
            DEBUG("erase channel(s)");
            // erase all marked channels
            for (network_channel_ptr channel : m_erased_channels) {
                erase_from(m_channels, channel);
                erase_from(m_peers_with_unwritten_data, channel);
                erase_from_if(m_peers, [=](const peer_map::value_type& kvp) {
                    return kvp.second == channel;
                });
            }
            m_erased_channels.clear();
        }
    }
    while (m_done == false);
    DEBUG("middleman done");
}

void middleman_loop(int pipe_fd, middleman_queue& queue) {
    DEBUG("run middleman loop");
    middleman mm;
    mm(pipe_fd, queue);
    DEBUG("middleman loop done");
}

} } // namespace cppa::detail
