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


#include "cppa/logging.hpp"
#include "cppa/to_string.hpp"
#include "cppa/singletons.hpp"

#include "cppa/io/middleman.hpp"
#include "cppa/io/buffered_writer.hpp"

namespace cppa { namespace io {

buffered_writer::buffered_writer(middleman* pptr, native_socket_type rfd, output_stream_ptr out)
: super(rfd, out->write_handle()), m_middleman(pptr)
, m_out(out), m_has_unwritten_data(false) { }

continue_writing_result buffered_writer::continue_writing() {
    CPPA_LOG_TRACE("");
    CPPA_LOG_DEBUG_IF(!m_has_unwritten_data, "nothing to write (done)");
    while (m_has_unwritten_data) {
        size_t written;
        try { written = m_out->write_some(m_buf.data(), m_buf.size()); }
        catch (std::exception& e) {
            CPPA_LOG_ERROR(to_verbose_string(e));
            static_cast<void>(e); // keep compiler happy
            return write_failure;
        }
        if (written != m_buf.size()) {
            CPPA_LOGMF(CPPA_DEBUG, self, "tried to write " << m_buf.size() << "bytes, "
                           << "only " << written << " bytes written");
            m_buf.erase_leading(written);
            return write_continue_later;
        }
        else {
            m_buf.clear();
            m_has_unwritten_data = false;
            CPPA_LOGMF(CPPA_DEBUG, self, "write done, " << written << "bytes written");
        }
    }
    return write_done;
}

void buffered_writer::write(size_t num_bytes, const void* data) {
    m_buf.write(num_bytes, data);
    register_for_writing();
}

void buffered_writer::register_for_writing() {
    if (!m_has_unwritten_data) {
        CPPA_LOGMF(CPPA_DEBUG, self, "register for writing");
        m_has_unwritten_data = true;
        m_middleman->continue_writer(this);
    }
}

} } // namespace cppa::network
