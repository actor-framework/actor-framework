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


#include <iostream>

#include "cppa/cppa.hpp"
#include "cppa/singletons.hpp"

#include "cppa/util/scope_guard.hpp"

#include "cppa/io/broker.hpp"
#include "cppa/io/middleman.hpp"
#include "cppa/io/broker.hpp"

#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/sync_request_bouncer.hpp"

using std::cout;
using std::endl;

namespace cppa { namespace io {

class broker_continuation {

 public:

    broker_continuation(broker_ptr ptr, const message_header& hdr, any_tuple&& msg)
    : m_self(std::move(ptr)), m_hdr(hdr), m_data(std::move(msg)) { }

    inline void operator()() {
        m_self->invoke_message(m_hdr, std::move(m_data));
    }

 private:

    broker_ptr     m_self;
    message_header m_hdr;
    any_tuple      m_data;

};

class default_broker_impl : public broker {

 public:

    typedef std::function<void (broker*)> function_type;

    default_broker_impl(function_type&& fun,
                        input_stream_ptr in,
                        output_stream_ptr out)
    : broker(in, out), m_fun(std::move(fun)) { }

    void init() override {
        enqueue(nullptr, make_any_tuple(atom("INITMSG")));
        become(
            on(atom("INITMSG")) >> [=] {
                unbecome();
                m_fun(this);
                auto mm = get_middleman();
                if (has_behavior()) mm->continue_reader(this);
                else if (not mm->has_writer(this)) {
                    CPPA_LOG_WARNING("spawn_io: no behavior was set and "
                                     "no data was written");
                    // middleman knows nothing about this broker;
                    // release its implicit reference count
                    deref();
                }
            }
        );
    }

 private:

    function_type m_fun;

};

void broker::invoke_message(const message_header& hdr, any_tuple msg) {
    if (exit_reason() != exit_reason::not_exited || m_bhvr_stack.empty()) {
        if (hdr.id.valid()) {
            detail::sync_request_bouncer srb{exit_reason()};
            srb(hdr.sender, hdr.id);
        }
        return;
    }
    // prepare actor for invocation of message handler
    m_dummy_node.sender = hdr.sender;
    m_dummy_node.msg = std::move(msg);
    m_dummy_node.mid = hdr.id;
    try {
        scoped_self_setter sss{this};
        auto& bhvr = m_bhvr_stack.back();
        if (not bhvr(m_dummy_node.msg)) {
            auto e = mailbox_element::create(hdr, std::move(m_dummy_node.msg));
            m_recv_policy.add_to_cache(e);
        }
    }
    catch (std::exception& e) {
        CPPA_LOG_ERROR("broker killed due to an unhandled exception: "
                       << to_verbose_string(e));
        // keep compiler happy in non-debug mode
        static_cast<void>(e);
        quit(exit_reason::unhandled_exception);
    }
    catch (...) {
        CPPA_LOG_ERROR("broker killed due to an unhandled exception");
        quit(exit_reason::unhandled_exception);
    }
    // restore dummy node
    m_dummy_node.sender.reset();
    m_dummy_node.msg.reset();
}

void broker::enqueue(const message_header& hdr, any_tuple msg) {
    get_middleman()->run_later(broker_continuation{this, hdr, std::move(msg)});
}

bool broker::initialized() const {
    return true;
}

void broker::quit(std::uint32_t reason) {
    cleanup(reason);
}

intrusive_ptr<broker> broker::from(std::function<void (broker*)> fun,
                                   input_stream_ptr in,
                                   output_stream_ptr out) {
    return make_counted<default_broker_impl>(std::move(fun), in, out);
}

broker::broker(input_stream_ptr in, output_stream_ptr out)
: super1(), super2(get_middleman(), in->read_handle(), std::move(out))
, m_is_continue_reading(false), m_disconnected(false), m_dirty(false)
, m_policy(at_least), m_policy_buffer_size(0), m_in(in)
, m_read(atom("IO_read"), static_cast<uint32_t>(in->read_handle())) {
    get_ref<2>(m_read).final_size(default_max_buffer_size);
    // acquire implicit reference count held by the middleman
    ref();
    // actor is running now
    get_actor_registry()->inc_running();
}

void broker::disconnect() {
    if (not m_disconnected) {
        m_disconnected = true;
        if (exit_reason() == exit_reason::not_exited) {
            auto msg = make_any_tuple(atom("IO_closed"),
                                      static_cast<uint32_t>(m_in->read_handle()));
            invoke_message(nullptr, std::move(msg));
        }
    }
}

void broker::cleanup(std::uint32_t reason) {
    if (not m_is_continue_reading) {
        // if we are currently in the continue_reading handler, we don't have
        // tell the MM to stop this actor, because the member function will
        // return stop_reading
        get_middleman()->stop_reader(this);
    }
    super1::cleanup(reason);
    get_actor_registry()->dec_running();
}

void broker::io_failed() {
    disconnect();
}

void broker::dispose() {
    deref(); // release implicit reference count of the middleman
}

void broker::receive_policy(policy_flag policy, size_t buffer_size) {
    CPPA_LOG_TRACE(CPPA_ARG(policy) << ", " << CPPA_ARG(buffer_size));
    if (not m_disconnected) {
        m_dirty = true;
        m_policy = policy;
        m_policy_buffer_size = buffer_size;
    }
}

continue_reading_result broker::continue_reading() {
    CPPA_LOG_TRACE("");
    m_is_continue_reading = true;
    auto sg = util::make_scope_guard([=] {
        m_is_continue_reading = false;
    });
    for (;;) {
        // stop reading if actor finished execution
        if (exit_reason() != exit_reason::not_exited) return read_closed;
        auto& buf = get_ref<2>(m_read);
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
            disconnect();
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
            invoke_message(nullptr, m_read);
            CPPA_LOG_INFO_IF(!m_read.vals()->unique(), "buffer became detached");
            get_ref<2>(m_read).clear();
        }
    }
}

void broker::write(size_t num_bytes, const void* data) {
    super2::write(num_bytes, data);
}

} } // namespace cppa::network
