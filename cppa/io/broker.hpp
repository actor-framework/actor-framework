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


#ifndef CPPA_BROKER_HPP
#define CPPA_BROKER_HPP

#include <functional>

#include "cppa/stackless.hpp"
#include "cppa/threadless.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/mailbox_element.hpp"

#include "cppa/io/buffered_writer.hpp"

#include "cppa/detail/fwd.hpp"

namespace cppa { namespace io {

class broker_continuation;

class broker : public extend<local_actor>::with<threadless, stackless>
             , public buffered_writer {

    typedef combined_type super1;
    typedef buffered_writer super2;

    friend class broker_continuation;

 public:

    enum policy_flag { at_least, at_most, exactly };

    broker(input_stream_ptr in, output_stream_ptr out);

    void io_failed() override;

    void dispose() override;

    void receive_policy(policy_flag policy, size_t buffer_size);

    continue_reading_result continue_reading() override;

    void write(size_t num_bytes, const void* data);

    void enqueue(const message_header& hdr, any_tuple msg);

    bool initialized() const;

    void quit(std::uint32_t reason);

    static intrusive_ptr<broker> from(std::function<void (broker*)> fun,
                                      input_stream_ptr in,
                                      output_stream_ptr out);

    template<typename F, typename T0, typename... Ts>
    static intrusive_ptr<broker> from(F fun,
                                      input_stream_ptr in,
                                      output_stream_ptr out,
                                      T0&& arg0,
                                      Ts&&... args) {
        return from(std::bind(std::move(fun),
                              std::placeholders::_1,
                              detail::fwd<T0>(arg0),
                              detail::fwd<Ts>(args)...),
                    std::move(in),
                    std::move(out));
    }

 protected:

    void cleanup(std::uint32_t reason);

 private:

    void invoke_message(const message_header& hdr, any_tuple msg);

    void disconnect();

    static constexpr size_t default_max_buffer_size = 65535;

    bool m_is_continue_reading;
    bool m_disconnected;
    bool m_dirty;
    policy_flag m_policy;
    size_t m_policy_buffer_size;
    input_stream_ptr m_in;
    cow_tuple<atom_value, uint32_t, util::buffer> m_read;

};

typedef intrusive_ptr<broker> broker_ptr;

} } // namespace cppa::network

#endif // CPPA_BROKER_HPP
