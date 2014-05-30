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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_IO_BUFFERED_WRITING_HPP
#define CPPA_IO_BUFFERED_WRITING_HPP

#include <utility>

#include "cppa/util/buffer.hpp"

#include "cppa/io/middleman.hpp"
#include "cppa/io/continuable.hpp"
#include "cppa/io/output_stream.hpp"

namespace cppa {
namespace io {

template<class Base, class Subtype>
class buffered_writing : public Base {

    typedef Base super;

 public:

    template<typename... Ts>
    buffered_writing(middleman* mm, output_stream_ptr out, Ts&&... args)
    : super{std::forward<Ts>(args)...}, m_parent{mm}, m_out{out}
    , m_has_unwritten_data{false} { }

    continue_writing_result continue_writing() override {
        CPPA_LOG_TRACE("");
        CPPA_LOG_DEBUG_IF(!m_has_unwritten_data, "nothing to write (done)");
        while (m_has_unwritten_data) {
            size_t written;
            try { written = m_out->write_some(m_buf.data(), m_buf.size()); }
            catch (std::exception& e) {
                CPPA_LOG_ERROR(to_verbose_string(e));
                static_cast<void>(e); // keep compiler happy
                return continue_writing_result::failure;
            }
            if (written != m_buf.size()) {
                CPPA_LOG_DEBUG("tried to write " << m_buf.size() << "bytes, "
                               << "only " << written << " bytes written");
                m_buf.erase_leading(written);
                return continue_writing_result::continue_later;
            }
            else {
                m_buf.clear();
                m_has_unwritten_data = false;
                CPPA_LOG_DEBUG("write done, " << written << " bytes written");
            }
        }
        return continue_writing_result::done;
    }

    inline bool has_unwritten_data() const {
        return m_has_unwritten_data;
    }

    void write(size_t num_bytes, const void* data) {
        m_buf.write(num_bytes, data);
        register_for_writing();
    }

    void write(const util::buffer& buf) {
        write(buf.size(), buf.data());
    }

    void write(util::buffer&& buf) {
        if (m_buf.empty()) m_buf = std::move(buf);
        else {
            m_buf.write(buf.size(), buf.data());
            buf.clear();
        }
        register_for_writing();
    }

    void register_for_writing() {
        if (!m_has_unwritten_data) {
            CPPA_LOG_DEBUG("register for writing");
            m_has_unwritten_data = true;
            m_parent->continue_writer(this);
        }
    }

    inline util::buffer& write_buffer() {
        return m_buf;
    }

 protected:

    inline middleman* parent() {
        return m_parent;
    }

    typedef buffered_writing combined_type;

 private:

    middleman* m_parent;
    output_stream_ptr m_out;
    bool m_has_unwritten_data;
    util::buffer m_buf;

};

} // namespace io
} // namespace cppa

#endif // CPPA_IO_BUFFERED_WRITING_HPP
