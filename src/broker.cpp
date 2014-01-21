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

#include "cppa/config.hpp"

#include "cppa/logging.hpp"
#include "cppa/singletons.hpp"

#include "cppa/util/scope_guard.hpp"

#include "cppa/io/broker.hpp"
#include "cppa/io/broker.hpp"
#include "cppa/io/middleman.hpp"
#include "cppa/io/buffered_writing.hpp"

#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/sync_request_bouncer.hpp"

using std::cout;
using std::endl;
using std::move;

namespace cppa {
namespace io {

namespace {

constexpr size_t default_max_buffer_size = 65535;

} // namespace <anonymous>

default_broker::default_broker(function_type f,
                               input_stream_ptr in,
                               output_stream_ptr out)
    : broker(std::move(in), std::move(out)), m_fun(std::move(f)) { }

default_broker::default_broker(function_type f, scribe_pointer ptr)
    : broker(std::move(ptr)), m_fun(std::move(f)) { }

default_broker::default_broker(function_type f, acceptor_uptr ptr)
    : broker(std::move(ptr)), m_fun(std::move(f)) { }

behavior default_broker::make_behavior() {
    CPPA_PUSH_AID(id());
    CPPA_LOG_TRACE("");
    enqueue({invalid_actor_addr, channel{this}},
            make_any_tuple(atom("INITMSG")));
    return (
        on(atom("INITMSG")) >> [=] {
            unbecome();
            m_fun(this);
        }
    );
}

class broker::continuation {

 public:

    continuation(broker_ptr ptr, const message_header& hdr, any_tuple&& msg)
    : m_self(move(ptr)), m_hdr(hdr), m_data(move(msg)) { }

    inline void operator()() {
        CPPA_PUSH_AID(m_self->id());
        CPPA_LOG_TRACE("");
        m_self->invoke_message(m_hdr, move(m_data));
    }

 private:

    broker_ptr     m_self;
    message_header m_hdr;
    any_tuple      m_data;

};

class broker::servant : public continuable {

    typedef continuable super;

 public:

    template<typename... Ts>
    servant(broker_ptr parent, Ts&&... args)
    : super{std::forward<Ts>(args)...}, m_disconnected{false}
    , m_broker{move(parent)} { }

    void io_failed(event_bitmask mask) override {
        if (mask == event::read) disconnect();
    }

    void dispose() override {
        m_broker->erase_io(read_handle());
        if (m_broker->m_io.empty() && m_broker->m_accept.empty()) {
            // release implicit reference count held by middleman
            // in caes no reader/writer is left for this broker
            m_broker->deref();
        }
    }

    void set_broker(broker_ptr new_broker) {
        if (!m_disconnected) m_broker = std::move(new_broker);
    }

 protected:

    void disconnect() {
        if (!m_disconnected) {
            m_disconnected = true;
            if (m_broker->exit_reason() == exit_reason::not_exited) {
                m_broker->invoke_message({invalid_actor_addr, invalid_actor},
                                         disconnect_message());
            }
        }
    }

    virtual any_tuple disconnect_message() = 0;

    bool m_disconnected;

    broker_ptr m_broker;

};

class broker::scribe : public extend<broker::servant>::with<buffered_writing> {

    typedef combined_type super;

 public:

    scribe(broker_ptr parent, input_stream_ptr in, output_stream_ptr out)
    : super{get_middleman(), out, move(parent), in->read_handle(), out->write_handle()}
    , m_is_continue_reading{false}, m_dirty{false}
    , m_policy{broker::at_least}, m_policy_buffer_size{0}, m_in{in}
    , m_read_msg{atom("IO_read"), connection_handle::from_int(in->read_handle())} {
        get_ref<2>(m_read_msg).final_size(default_max_buffer_size);
    }

    void receive_policy(broker::policy_flag policy, size_t buffer_size) {
        CPPA_LOG_TRACE(CPPA_ARG(policy) << ", " << CPPA_ARG(buffer_size));
        if (not m_disconnected) {
            m_dirty = true;
            m_policy = policy;
            m_policy_buffer_size = buffer_size;
        }
    }

    continue_reading_result continue_reading() override {
        CPPA_LOG_TRACE("");
        m_is_continue_reading = true;
        auto sg = util::make_scope_guard([=] {
            m_is_continue_reading = false;
        });
        for (;;) {
            // stop reading if actor finished execution
            if (m_broker->exit_reason() != exit_reason::not_exited) {
                return read_closed;
            }
            auto& buf = get_ref<2>(m_read_msg);
            if (m_dirty) {
                m_dirty = false;
                if (m_policy == broker::at_most || m_policy == broker::exactly) {
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
               || (m_policy == broker::exactly && buf.size() != m_policy_buffer_size)) {
                return read_continue_later;
            }
            if  ( (   m_policy == broker::at_least
                   && buf.size() >= m_policy_buffer_size)
               || m_policy == broker::exactly
               || m_policy == broker::at_most) {
                CPPA_LOG_DEBUG("invoke io actor");
                m_broker->invoke_message({invalid_actor_addr, nullptr}, m_read_msg);
                CPPA_LOG_INFO_IF(!m_read_msg.vals()->unique(), "detached buffer");
                get_ref<2>(m_read_msg).clear();
            }
        }
    }

    connection_handle id() const {
        return connection_handle::from_int(m_in->read_handle());
    }

 protected:

    any_tuple disconnect_message() override {
        return make_any_tuple(atom("IO_closed"),
                              connection_handle::from_int(m_in->read_handle()));
    }

 private:

    bool m_is_continue_reading;
    bool m_dirty;
    broker::policy_flag m_policy;
    size_t m_policy_buffer_size;
    input_stream_ptr m_in;
    cow_tuple<atom_value, connection_handle, util::buffer> m_read_msg;

};

class broker::doorman : public broker::servant {

    typedef servant super;

 public:

    doorman(broker_ptr parent, acceptor_uptr ptr)
    : super{move(parent), ptr->file_handle()}
    , m_accept_msg{atom("IO_accept"),
                   accept_handle::from_int(ptr->file_handle())} {
        m_ptr.reset(ptr.release());
    }

    continue_reading_result continue_reading() override {
        CPPA_LOG_TRACE("");
        for (;;) {
            optional<stream_ptr_pair> opt{none};
            try { opt = m_ptr->try_accept_connection(); }
            catch (std::exception& e) {
                CPPA_LOG_ERROR(to_verbose_string(e));
                static_cast<void>(e); // keep compiler happy
                return read_failure;
            }
            if (opt) {
                using namespace std;
                auto& p = *opt;
                get_ref<2>(m_accept_msg) = m_broker->add_scribe(move(p.first),
                                                                move(p.second));
                m_broker->invoke_message({invalid_actor_addr, nullptr}, m_accept_msg);
            }
            else return read_continue_later;
       }
    }

 protected:

    any_tuple disconnect_message() override {
        return make_any_tuple(atom("IO_closed"),
                              accept_handle::from_int(m_ptr->file_handle()));
    }

 private:

    acceptor_uptr m_ptr;
    cow_tuple<atom_value, accept_handle, connection_handle> m_accept_msg;

};

void broker::invoke_message(const message_header& hdr, any_tuple msg) {
    CPPA_LOG_TRACE(CPPA_TARG(msg, to_string));
    if (planned_exit_reason() != exit_reason::not_exited || bhvr_stack().empty()) {
        CPPA_LOG_DEBUG("actor already finished execution"
                       << ", planned_exit_reason = " << planned_exit_reason()
                       << ", bhvr_stack().empty() = " << bhvr_stack().empty());
        if (hdr.id.valid()) {
            detail::sync_request_bouncer srb{exit_reason()};
            srb(hdr.sender, hdr.id);
        }
        return;
    }
    // prepare actor for invocation of message handler
    m_dummy_node.sender = hdr.sender;
    m_dummy_node.msg = move(msg);
    m_dummy_node.mid = hdr.id;
    try {
        auto bhvr = bhvr_stack().back();
        auto mid = bhvr_stack().back_id();
        switch (m_invoke_policy.handle_message(this, &m_dummy_node, bhvr, mid)) {
            case policy::hm_msg_handled: {
                CPPA_LOG_DEBUG("handle_message returned hm_msg_handled");
                while (   !bhvr_stack().empty()
                       && planned_exit_reason() == exit_reason::not_exited
                       && invoke_message_from_cache()) {
                    // rinse and repeat
                }
                break;
            }
            case policy::hm_drop_msg:
                CPPA_LOG_DEBUG("handle_message returned hm_drop_msg");
                break;
            case policy::hm_skip_msg:
            case policy::hm_cache_msg: {
                CPPA_LOG_DEBUG("handle_message returned hm_skip_msg or hm_cache_msg");
                auto e = mailbox_element::create(hdr, move(m_dummy_node.msg));
                m_priority_policy.push_to_cache(unique_mailbox_element_pointer{e});
                break;
            }
            default: CPPA_CRITICAL("illegal result of handle_message");
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
    m_dummy_node.sender = actor_addr{};
    m_dummy_node.msg.reset();
    // cleanup if needed
    if (planned_exit_reason() != exit_reason::not_exited) {
        cleanup(planned_exit_reason());
    }
    else if (bhvr_stack().empty()) {
        CPPA_LOG_DEBUG("bhvr_stack().empty(), quit for normal exit reason");
        quit(exit_reason::normal);
        cleanup(planned_exit_reason());
    }
}

bool broker::invoke_message_from_cache() {
    CPPA_LOG_TRACE("");
    auto bhvr = bhvr_stack().back();
    auto mid = bhvr_stack().back_id();
    auto e = m_priority_policy.cache_end();
    CPPA_LOG_DEBUG(std::distance(m_priority_policy.cache_begin(), e)
                   << " elements in cache");
    for (auto i = m_priority_policy.cache_begin(); i != e; ++i) {
        auto res = m_invoke_policy.invoke_message(this, *i, bhvr, mid);
        if (res || !*i) {
            m_priority_policy.cache_erase(i);
            if (res) return true;
            return invoke_message_from_cache();
        }
    }
    return false;
}

void broker::enqueue(const message_header& hdr, any_tuple msg) {
    get_middleman()->run_later(continuation{this, hdr, move(msg)});
}

bool broker::initialized() const {
    return true;
}

void broker::init_broker() {
    // acquire implicit reference count held by the middleman
    ref();
    // actor is running now
    get_actor_registry()->inc_running();
}

broker::broker(input_stream_ptr in, output_stream_ptr out) {
    using namespace std;
    init_broker();
    add_scribe(move(in), move(out));
}

broker::broker(scribe_pointer ptr) {
    using namespace std;
    init_broker();
    auto id = ptr->id();
    m_io.insert(make_pair(id, move(ptr)));
}

broker::broker(acceptor_uptr ptr) {
    using namespace std;
    init_broker();
    add_doorman(move(ptr));
}

void broker::cleanup(std::uint32_t reason) {
    super::cleanup(reason);
    get_actor_registry()->dec_running();
}

void broker::write(const connection_handle& hdl, size_t num_bytes, const void* buf) {
    auto i = m_io.find(hdl);
    if (i != m_io.end()) i->second->write(num_bytes, buf);
}

void broker::write(const connection_handle& hdl, const util::buffer& buf) {
    write(hdl, buf.size(), buf.data());
}

void broker::write(const connection_handle& hdl, util::buffer&& buf) {
    write(hdl, buf.size(), buf.data());
    buf.clear();
}

broker_ptr init_and_launch(broker_ptr ptr) {
    CPPA_PUSH_AID(ptr->id());
    CPPA_LOGF_TRACE("init and launch actor with id " << ptr->id());
    // continue reader only if not inherited from default_broker_impl
    auto mm = get_middleman();
    mm->run_later([=] {
        CPPA_LOGC_TRACE("NONE", "init_and_launch::run_later_functor", "");
        CPPA_LOGF_WARNING_IF(ptr->m_io.empty() && ptr->m_accept.empty(),
                             "both m_io and m_accept are empty");
        // 'launch' all backends
        CPPA_LOGC_DEBUG("NONE", "init_and_launch::run_later_functor",
                        "add " << ptr->m_io.size() << " IO servants");
        for (auto& kvp : ptr->m_io)
            mm->continue_reader(kvp.second.get());
        CPPA_LOGC_DEBUG("NONE", "init_and_launch::run_later_functor",
                        "add " << ptr->m_accept.size() << " acceptors");
        for (auto& kvp : ptr->m_accept)
            mm->continue_reader(kvp.second.get());
    });
    // exec initialization code
    auto bhvr = ptr->make_behavior();
    if (bhvr) ptr->become(std::move(bhvr));
    CPPA_LOGF_WARNING_IF(!ptr->has_behavior(), "broker w/o behavior spawned");
    return ptr;
}

broker_ptr broker::from_impl(std::function<void (broker*)> fun,
                             input_stream_ptr in,
                             output_stream_ptr out) {
    return make_counted<default_broker>(move(fun), move(in), move(out));
}

broker_ptr broker::from(std::function<void (broker*)> fun, acceptor_uptr in) {
    return make_counted<default_broker>(move(fun), move(in));
}

void broker::erase_io(int id) {
    m_io.erase(connection_handle::from_int(id));
}

void broker::erase_acceptor(int id) {
    m_accept.erase(accept_handle::from_int(id));
}

connection_handle broker::add_scribe(input_stream_ptr in, output_stream_ptr out) {
    using namespace std;
    auto id = connection_handle::from_int(in->read_handle());
    m_io.insert(make_pair(id, create_unique<scribe>(this, move(in), move(out))));
    return id;
}

accept_handle broker::add_doorman(acceptor_uptr ptr) {
    using namespace std;
    auto id = accept_handle::from_int(ptr->file_handle());
    m_accept.insert(make_pair(id, create_unique<doorman>(this, move(ptr))));
    return id;
}

actor broker::fork_impl(std::function<void (broker*)> fun,
                        connection_handle hdl) {
    CPPA_LOG_TRACE(CPPA_MARG(hdl, id));
    auto i = m_io.find(hdl);
    if (i == m_io.end()) {
        CPPA_LOG_ERROR("invalid handle");
        throw std::invalid_argument("invalid handle");
    }
    scribe* sptr = i->second.get(); // non-owning pointer
    auto result = make_counted<default_broker>(move(fun), move(i->second));
    init_and_launch(result);
    sptr->set_broker(result); // set new broker
    m_io.erase(i);
    return {result};
}

void broker::receive_policy(const connection_handle& hdl,
                            broker::policy_flag policy,
                            size_t buffer_size) {
    auto i = m_io.find(hdl);
    if (i != m_io.end()) i->second->receive_policy(policy, buffer_size);
}

broker::~broker() {
    CPPA_LOG_TRACE("");
}

} // namespace io
} // namespace cppa
