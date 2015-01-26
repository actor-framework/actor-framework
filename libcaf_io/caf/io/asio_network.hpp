/******************************************************************************\
 *                                                                            *
 *           ____                  _        _        _                        *
 *          | __ )  ___   ___  ___| |_     / \   ___| |_ ___  _ __            *
 *          |  _ \ / _ \ / _ \/ __| __|   / _ \ / __| __/ _ \| '__|           *
 *          | |_) | (_) | (_) \__ \ |_ _ / ___ \ (__| || (_) | |              *
 *          |____/ \___/ \___/|___/\__(_)_/   \_\___|\__\___/|_|              *
 *                                                                            *
 *                                                                            *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CAF_IO_ASIO_NETWORK_HPP
#define CAF_IO_ASIO_NETWORK_HPP

#include <vector>
#include <string>
#include <cstdint>

#include "boost/asio.hpp"

#include "caf/exception.hpp"
#include "caf/ref_counted.hpp"

#include "caf/io/receive_policy.hpp"

#include "caf/detail/logging.hpp"

namespace boost {
namespace actor_io {
namespace network {

/**
 * @brief Low-level backend for IO multiplexing.
 */
using multiplexer = asio::io_service;

/**
 * @brief Gets the multiplexer singleton.
 */
multiplexer& get_multiplexer_singleton();

/**
 * @brief Makes sure a {@link multiplexer} does not stop its event loop
 *        before the application requests a shutdown.
 *
 * The supervisor informs the multiplexer in its constructor that it
 * must not exit the event loop until the destructor of the supervisor
 * has been called.
 */
using supervisor = asio::io_service::work;

/**
 * @brief Low-level socket type used as default.
 */
using default_socket = asio::ip::tcp::socket;

/**
 * @brief Low-level socket type used as default.
 */
using default_socket_acceptor = asio::ip::tcp::acceptor;

/**
 * @brief Platform-specific native socket type.
 */
using native_socket = typename default_socket::native_handle_type;

/**
 * @brief Platform-specific native acceptor socket type.
 */
using native_socket_acceptor = typename default_socket_acceptor::native_handle_type;

/**
 * @brief Identifies network IO operations, i.e., read or write.
 */
enum class operation {
    read,
    write
};

/**
 * @brief A manager configures an IO device and provides callbacks
 *        for various IO operations.
 */
class manager : public actor::ref_counted {

 public:

    virtual ~manager();

    /**
     * @brief Called during application shutdown, indicating that the manager
     *        should cause its underlying IO device to stop read IO operations.
     */
    virtual void stop_reading() = 0;

    /**
     * @brief Causes the manager to stop all IO operations on its IO device.
     */
    virtual void stop() = 0;

    /**
     * @brief Called by the underlying IO device to report failures.
     */
    virtual void io_failure(operation op, const std::string& error_message) = 0;
};

// functions from ref_counted cannot be found by ADL => provide new overload
inline void intrusive_ptr_add_ref(manager* p) {
    p->ref();
}

// functions from ref_counted cannot be found by ADL => provide new overload
inline void intrusive_ptr_release(manager* p) {
    p->deref();
}

/**
 * @relates manager
 */
using manager_ptr = intrusive_ptr<manager>;

/**
 * @brief A stream manager configures an IO stream and provides callbacks
 *        for incoming data as well as for error handling.
 */
class stream_manager : public manager {

 public:

    virtual ~stream_manager();

    /**
     * @brief Called by the underlying IO device whenever it received data.
     */
    virtual void consume(const void* data, size_t num_bytes) = 0;

};

/**
 * @brief A stream capable of both reading and writing. The stream's input
 *        data is forwarded to its {@link stream_manager manager}.
 */
template<class Socket>
class stream {

 public:

    /**
     * @brief A smart pointer to a stream manager.
     */
    using manager_ptr = intrusive_ptr<stream_manager>;

    /**
     * @brief A buffer class providing a compatible
     *        interface to @p std::vector.
     */
    using buffer_type = std::vector<char>;

    stream(multiplexer& backend) : m_writing(false), m_fd(backend) {
        configure_read(receive_policy::at_most(1024));
    }

    /**
     * @brief Returns the @p multiplexer this stream belongs to.
     */
    inline multiplexer& backend() {
        return m_fd.get_io_service();
    }

    /**
     * @brief Returns the IO socket.
     */
    inline Socket& socket_handle() {
        return m_fd;
    }

    /**
     * @brief Initializes this stream, setting the socket handle to @p fd.
     */
    void init(Socket fd) {
        m_fd = std::move(fd);
    }

    /**
     * @brief Starts reading data from the socket, forwarding incoming
     *        data to @p mgr.
     */
    void start(const manager_ptr& mgr) {
        BOOST_ACTOR_REQUIRE(mgr != nullptr);
        read_loop(mgr);
    }

    /**
     * @brief Configures how much data will be provided
     *        for the next @p consume callback.
     * @warning Must not be called outside the IO multiplexers event loop
     *          once the stream has been started.
     */
    void configure_read(receive_policy::config config) {
        m_rd_flag = config.first;
        m_rd_size = config.second;
    }

    /**
     * @brief Copies data to the write buffer.
     * @note Not thread safe.
     */
    void write(const void* buf, size_t num_bytes) {
        auto first = reinterpret_cast<const char*>(buf);
        auto last  = first + num_bytes;
        m_wr_offline_buf.insert(first, last);
    }

    /**
     * @brief Returns the write buffer of this stream.
     * @warning Must not be modified outside the IO multiplexers event loop
     *          once the stream has been started.
     */
    buffer_type& wr_buf() {
        return m_wr_offline_buf;
    }

    buffer_type& rd_buf() {
        return m_rd_buf;
    }

    /**
     * @brief Sends the content of the write buffer, calling
     *        the @p io_failure member function of @p mgr in
     *        case of an error.
     * @warning Must not be called outside the IO multiplexers event loop
     *          once the stream has been started.
     */
    void flush(const manager_ptr& mgr) {
        BOOST_ACTOR_REQUIRE(mgr != nullptr);
        if (!m_wr_offline_buf.empty() && !m_writing) {
            m_writing = true;
            write_loop(mgr);
        }
    }

    /**
     * @brief Closes the network connection, thus stopping this stream.
     */
    void stop() {
        BOOST_ACTOR_LOGM_TRACE("boost::actor_io::network::stream", "");
        m_fd.close();
    }

    void stop_reading() {
        BOOST_ACTOR_LOGM_TRACE("boost::actor_io::network::stream", "");
        system::error_code ec; // ignored
        m_fd.shutdown(asio::ip::tcp::socket::shutdown_receive, ec);
    }

 private:

    void write_loop(const manager_ptr& mgr) {
        if (m_wr_offline_buf.empty()) {
            m_writing = false;
            return;
        }
        m_wr_buf.clear();
        m_wr_buf.swap(m_wr_offline_buf);
        asio::async_write(m_fd, asio::buffer(m_wr_buf),
                          [=](const system::error_code& ec, size_t nb) {
            BOOST_ACTOR_LOGC_TRACE("boost::actor_io::network::stream",
                                   "write_loop$lambda",
                                   BOOST_ACTOR_ARG(this));
            static_cast<void>(nb); // silence compiler warning
            if (!ec) {
                BOOST_ACTOR_LOGC_DEBUG("boost::actor_io::network::stream",
                                       "write_loop$lambda",
                                       nb << " bytes sent");
                write_loop(mgr);
            }
            else {
                BOOST_ACTOR_LOGC_DEBUG("boost::actor_io::network::stream",
                                       "write_loop$lambda",
                                       "error during send: " << ec.message());
                mgr->io_failure(operation::read, ec.message());
                m_writing = false;
            }
        });
    }

    void read_loop(const manager_ptr& mgr) {
        auto cb = [=](const system::error_code& ec, size_t read_bytes) {
            BOOST_ACTOR_LOGC_TRACE("boost::actor_io::network::stream",
                                   "read_loop$cb",
                                   BOOST_ACTOR_ARG(this));
            if (!ec) {
                mgr->consume(m_rd_buf.data(), read_bytes);
                read_loop(mgr);
            }
            else mgr->io_failure(operation::read, ec.message());
        };
        switch (m_rd_flag) {
            case receive_policy_flag::exactly:
                if (m_rd_buf.size() < m_rd_size) m_rd_buf.resize(m_rd_size);
                asio::async_read(m_fd, asio::buffer(m_rd_buf, m_rd_size), cb);
                break;
            case receive_policy_flag::at_most:
                if (m_rd_buf.size() < m_rd_size) m_rd_buf.resize(m_rd_size);
                m_fd.async_read_some(asio::buffer(m_rd_buf, m_rd_size), cb);
                break;
            case receive_policy_flag::at_least: {
                // read up to 10% more, but at least allow 100 bytes more
                auto min_size =   m_rd_size
                                + std::max<size_t>(100, m_rd_size / 10);
                if (m_rd_buf.size() < min_size) m_rd_buf.resize(min_size);
                collect_data(mgr, 0);
                break;
            }
        }
    }

    void collect_data(const manager_ptr& mgr, size_t collected_bytes) {
        m_fd.async_read_some(asio::buffer(m_rd_buf.data() + collected_bytes,
                                          m_rd_buf.size() - collected_bytes),
                            [=](const system::error_code& ec, size_t nb) {
            BOOST_ACTOR_LOGC_TRACE("boost::actor_io::network::stream",
                                   "collect_data$lambda",
                                   BOOST_ACTOR_ARG(this));
            if (!ec) {
                auto sum = collected_bytes + nb;
                if (sum >= m_rd_size) {
                    mgr->consume(m_rd_buf.data(), sum);
                    read_loop(mgr);
                }
                else collect_data(mgr, sum);
            }
            else mgr->io_failure(operation::write, ec.message());
        });
    }

    bool                m_writing;
    Socket              m_fd;
    receive_policy_flag m_rd_flag;
    size_t              m_rd_size;
    buffer_type         m_rd_buf;
    buffer_type         m_wr_buf;
    buffer_type         m_wr_offline_buf;

};

/**
 * @brief An acceptor manager configures an acceptor and provides
 *        callbacks for incoming connections as well as for error handling.
 */
class acceptor_manager : public manager {

 public:

    /**
     * @brief Called by the underlying IO device to indicate that
     *        a new connection is awaiting acceptance.
     */
    virtual void new_connection() = 0;

};

/**
 * @brief An acceptor is responsible for accepting incoming connections.
 */
template<class SocketAcceptor>
class acceptor {

    using protocol_type = typename SocketAcceptor::protocol_type;
    using socket_type   = asio::basic_stream_socket<protocol_type>;

 public:

    /**
     * @brief A manager providing the @p accept member function.
     */
    using manager_type = acceptor_manager;

    /**
     * @brief A smart pointer to an acceptor manager.
     */
    using manager_ptr = intrusive_ptr<manager_type>;

    acceptor(multiplexer& backend)
    : m_backend(backend), m_accept_fd(backend), m_fd(backend) { }

    /**
     * @brief Returns the @p multiplexer this acceptor belongs to.
     */
    inline multiplexer& backend() {
        return m_backend;
    }

    /**
     * @brief Returns the IO socket.
     */
    inline SocketAcceptor& socket_handle() {
        return m_accept_fd;
    }

    /**
     * @brief Returns the accepted socket. This member function should
     *        be called only from the @p new_connection callback.
     */
    inline socket_type& accepted_socket() {
        return m_fd;
    }

    /**
     * @brief Initializes this acceptor, setting the socket handle to @p fd.
     */
    void init(SocketAcceptor fd) {
        m_accept_fd = std::move(fd);
    }

    /**
     * @brief Starts this acceptor, forwarding all incoming connections to
     *        @p manager. The intrusive pointer will be released after the
     *        acceptor has been closed or an IO error occured.
     */
    void start(const manager_ptr& mgr) {
        BOOST_ACTOR_REQUIRE(mgr != nullptr);
        accept_loop(mgr);
    }

    /**
     * @brief Closes the network connection, thus stopping this acceptor.
     */
    void stop() {
        m_accept_fd.close();
    }

 private:

    void accept_loop(const manager_ptr& mgr) {
        m_accept_fd.async_accept(m_fd, [=](const system::error_code& ec) {
            BOOST_ACTOR_LOGM_TRACE("boost::actor_io::network::acceptor", "");
            if (!ec) {
                mgr->new_connection(); // probably moves m_fd
                // reset m_fd for next accept operation
                m_fd = socket_type{m_accept_fd.get_io_service()};
                accept_loop(mgr);
            }
            else mgr->io_failure(operation::read, ec.message());
        });
    }

    multiplexer&   m_backend;
    SocketAcceptor m_accept_fd;
    socket_type    m_fd;

};

template<class Socket>
void ipv4_connect(Socket& fd, const std::string& host, uint16_t port) {
    CAF_LOGF_TRACE(CAF_ARG(host) << ", " CAF_ARG(port));
    using asio::ip::tcp;
    try {
        tcp::resolver r(fd.get_io_service());
        tcp::resolver::query q(tcp::v4(), host, std::to_string(port));
        auto i = r.resolve(q);
        asio::connect(fd, i);
    }
    catch (system::system_error& se) {
        throw actor::network_error(se.code().message());
    }
}

inline default_socket new_ipv4_connection(const std::string& host,
                                          uint16_t port) {
    default_socket fd{get_multiplexer_singleton()};
    ipv4_connect(fd, host, port);
    return fd;
}

template<class SocketAcceptor>
void ipv4_bind(SocketAcceptor& fd,
               uint16_t port,
               const char* addr = nullptr) {
    BOOST_ACTOR_LOGF_TRACE(BOOST_ACTOR_ARG(port));
    using asio::ip::tcp;
    try {
        auto bind_and_listen = [&](tcp::endpoint& ep) {
            fd.open(ep.protocol());
            fd.set_option(tcp::acceptor::reuse_address(true));
            fd.bind(ep);
            fd.listen();
        };
        if (addr) {
            tcp::endpoint ep(asio::ip::address::from_string(addr), port);
            bind_and_listen(ep);
        }
        else {
            tcp::endpoint ep(tcp::v4(), port);
            bind_and_listen(ep);
        }
    }
    catch (system::system_error& se) {
        if (se.code() == system::errc::address_in_use) {
            throw actor::bind_failure(se.code().message());
        }
        throw actor::network_error(se.code().message());
    }
}

inline default_socket_acceptor new_ipv4_acceptor(uint16_t port,
                                                 const char* addr = nullptr) {
    default_socket_acceptor fd{get_multiplexer_singleton()};
    ipv4_bind(fd, port, addr);
    return fd;
}

} // namespace network
} // namespace actor_io
} // namespace boost

#endif // CAF_IO_ASIO_NETWORK_HPP
