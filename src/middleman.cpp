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


#include "cppa/actor.hpp"
#include "cppa/config.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/uniform_type_info.hpp"
#include "cppa/process_information.hpp"

#include "cppa/detail/middleman.hpp"
#include "cppa/detail/addressed_message.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"

#include "cppa/util/buffer.hpp"
#include "cppa/util/input_stream.hpp"
#include "cppa/util/output_stream.hpp"

#define DEBUG(arg)                                                             \
    {                                                                          \
        std::ostringstream oss;                                                \
        oss << "[process id: "                                                 \
            << cppa::process_information::get()->process_id()                  \
            << "] " << arg << std::endl;                                       \
        std::cout << oss.str();                                                \
    } (void) 0

#undef DEBUG
#define DEBUG(unused) ((void) 0)

namespace cppa { namespace detail {

namespace { const size_t ui32_size = sizeof(std::uint32_t); }

class middleman;

class connection {

 public:

    connection(middleman* ptr, native_socket_type ifd, native_socket_type ofd)
    : m_parent(ptr), m_has_unwritten_data(false)
    , m_read_handle(ifd), m_write_handle(ofd) { }

    virtual bool continue_reading() = 0;

    virtual bool continue_writing() = 0;

    virtual void write(const addressed_message& msg);

    inline native_socket_type read_handle() const {
        return m_read_handle;
    }

    inline native_socket_type write_handle() const {
        return m_write_handle;
    }

    virtual bool is_acceptor_of(const actor_ptr&) const {
        return false;
    }

    inline bool has_unwritten_data() const {
        return m_has_unwritten_data;
    }

 protected:

    inline middleman* parent() { return m_parent; }

    inline const middleman* parent() const { return m_parent; }

    inline void has_unwritten_data(bool value) {
        m_has_unwritten_data = value;
    }

 private:

    middleman* m_parent;
    bool m_has_unwritten_data;
    native_socket_type m_read_handle;
    native_socket_type m_write_handle;

};

class peer_connection : public connection {

    typedef connection super;

 public:

    peer_connection(middleman* parent,
                    util::input_stream_ptr istream,
                    util::output_stream_ptr ostream,
                    process_information_ptr peer_ptr = nullptr)
    : super(parent, istream->read_file_handle(), ostream->write_file_handle())
    , m_istream(istream), m_ostream(ostream), m_peer(peer_ptr)
    , m_rd_state((peer_ptr) ? wait_for_msg_size : wait_for_process_info)
    , m_meta_msg(uniform_typeid<addressed_message>()) {
        m_rd_buf.reset(m_rd_state == wait_for_process_info
                       ? ui32_size + process_information::node_id_size
                       : ui32_size);
    }

    ~peer_connection() {
        if (m_peer) {
            // collect all children (proxies to actors of m_peer)
            std::vector<actor_proxy_ptr> children;
            children.reserve(20);
            get_actor_proxy_cache().erase_all(m_peer->node_id(),
                                              m_peer->process_id(),
                                              [&](actor_proxy_ptr& pptr) {
                children.push_back(std::move(pptr));
            });
            // kill all proxies
            for (actor_proxy_ptr& pptr: children) {
                pptr->enqueue(nullptr,
                              make_any_tuple(atom("KILL_PROXY"),
                                             exit_reason::remote_link_unreachable));
            }
        }
    }

    bool continue_reading() {
        for (;;) {
            try { m_rd_buf.append_from(m_istream.get()); }
            catch (std::exception& e) {
                DEBUG(e.what());
                return false;
            }
            if (!m_rd_buf.full()) return true; // try again later
        }
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

    util::buffer m_rd_buf;
    util::buffer m_wr_buf;

};

} } // namespace cppa::detail
