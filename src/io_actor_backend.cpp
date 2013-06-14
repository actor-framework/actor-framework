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


#include "cppa/singletons.hpp"

#include "cppa/network/middleman.hpp"
#include "cppa/network/io_actor_backend.hpp"

#include "cppa/detail/actor_registry.hpp"

namespace cppa { namespace network {

io_actor_backend::io_actor_backend(input_stream_ptr in,
                                   output_stream_ptr out,
                                   io_actor_ptr ptr)
: super(get_middleman(), in->read_handle(), std::move(out))
, m_in(in), m_self(ptr) {
    m_self->m_parent = this;
    get_ref<0>(m_read) = atom("IO_read");
    get_ref<1>(m_read).final_size(default_max_buffer_size);
    m_dirty = false;
    m_policy = at_least;
    m_policy_buffer_size = 0;
}

io_actor_backend::~io_actor_backend() {
    handle_disconnect();
    get_actor_registry()->dec_running();
}

void io_actor_backend::init() {
    get_actor_registry()->inc_running();
    auto selfptr = m_self.get();
    scoped_self_setter sss{selfptr};
    selfptr->init();
}

void io_actor_backend::handle_disconnect() {
    if (m_self) {
        CPPA_LOG_DEBUG("became disconnected");
        if (m_self->exit_reason() == exit_reason::not_exited) {
            m_self->invoke_message(make_any_tuple(atom("IO_closed")));
        }
        m_self.reset();
    }
}

void io_actor_backend::io_failed() {
    handle_disconnect();
}

void io_actor_backend::receive_policy(policy_flag policy, size_t buffer_size) {
    CPPA_LOG_TRACE(CPPA_ARG(policy) << ", " << CPPA_ARG(buffer_size));
    m_dirty = true;
    m_policy = policy;
    m_policy_buffer_size = buffer_size;
}

continue_reading_result io_actor_backend::continue_reading() {
    CPPA_LOG_TRACE("");
    for (;;) {
        auto& buf = get_ref<1>(m_read);
        if (m_dirty) {
            m_dirty = false;
            if (m_policy == at_most || m_policy == exactly) {
                buf.final_size(m_policy_buffer_size);
            }
            else buf.final_size(default_max_buffer_size);
        }
        auto before = buf.size();
        try { buf.append_from(m_in.get()); }
        catch (std::ios_base::failure&) {
            handle_disconnect();
            return read_failure;
        }
        CPPA_LOG_DEBUG("received " << (buf.size() - before) << " bytes");
        if  ( before == buf.size()
           || (m_policy == exactly && buf.size() != m_policy_buffer_size)) {
            return read_continue_later;
        }
        if  ( (m_policy == at_least && buf.size() >= m_policy_buffer_size)
           || m_policy == exactly
           || m_policy == at_most) {
            CPPA_LOG_DEBUG("invoke io actor");
            m_self->invoke_message(m_read);
            CPPA_LOG_INFO_IF(!m_read.vals()->unique(), "buffer became detached");
            if (m_self == nullptr) {
                // io_actor::quit() calls handle_disconnect, which sets
                // m_self to nullptr
                return read_closed;
            }
            get_ref<1>(m_read).clear();
        }
    }
}

void io_actor_backend::close() {
    CPPA_LOG_DEBUG("");
    get_middleman()->stop_reader(this);
}

void io_actor_backend::write(size_t num_bytes, const void* data) {
    super::write(num_bytes, data);
}

} } // namespace cppa::network
