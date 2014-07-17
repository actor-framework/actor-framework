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

#ifndef CAF_IO_NETWORK_HPP
#define CAF_IO_NETWORK_HPP

#include <thread>
#include <vector>
#include <string>
#include <cstdint>

#include "caf/config.hpp"
#include "caf/extend.hpp"
#include "caf/exception.hpp"
#include "caf/ref_counted.hpp"

#include "caf/mixin/memory_cached.hpp"

#include "caf/io/fwd.hpp"
#include "caf/io/accept_handle.hpp"
#include "caf/io/receive_policy.hpp"
#include "caf/io/connection_handle.hpp"

#include "caf/detail/logging.hpp"

#ifdef CAF_WINDOWS
#   include <w32api.h>
#   undef _WIN32_WINNT
#   undef WINVER
#   define _WIN32_WINNT WindowsVista
#   define WINVER WindowsVista
#   include <ws2tcpip.h>
#   include <winsock2.h>
#   include <ws2ipdef.h>
#else
#   include <unistd.h>
#   include <errno.h>
#   include <sys/socket.h>
#endif

// poll vs epoll backend
#if !defined(CAF_LINUX) || defined(CAF_POLL_IMPL) // poll() multiplexer
#   define CAF_POLL_MULTIPLEXER
#   ifndef CAF_WINDOWS
#       include <poll.h>
#   endif // CAF_WINDOWS
#   ifndef POLLRDHUP
#       define POLLRDHUP POLLHUP
#   endif
    namespace caf {
    namespace io {
    namespace network {
        constexpr short input_mask  = POLLIN | POLLPRI;
        constexpr short error_mask  = POLLRDHUP | POLLERR | POLLHUP | POLLNVAL;
        constexpr short output_mask = POLLOUT;
        class event_handler;
        using multiplexer_data = pollfd;
        using multiplexer_poll_shadow_data = std::vector<event_handler*>;
    } // namespace network
    } // namespace io
    } // namespace caf
#else
#   define CAF_EPOLL_MULTIPLEXER
#   include <sys/epoll.h>
    namespace caf {
    namespace io {
    namespace network {
        constexpr int input_mask  = EPOLLIN;
        constexpr int error_mask  = EPOLLRDHUP | EPOLLERR | EPOLLHUP;
        constexpr int output_mask = EPOLLOUT;
        using multiplexer_data = epoll_event;
        using multiplexer_poll_shadow_data = int;
    } // namespace network
    } // namespace io
    } // namespace caf
#endif

namespace caf {
namespace io {
namespace network {

// some more annoying platform-dependent bootstrapping
#ifdef CAF_WINDOWS
    using native_socket_t = SOCKET;
    using setsockopt_ptr = const char*;
    using socket_send_ptr = const char*;
    using socket_recv_ptr = char*;
    using socklen_t = int;
    constexpr native_socket_t invalid_socket = INVALID_SOCKET;
    inline int last_socket_error() { return WSAGetLastError(); }
    inline bool would_block_or_temporarily_unavailable(int errcode) {
        return errcode == WSAEWOULDBLOCK || errcode == WSATRY_AGAIN;
    }
    constexpr int ec_out_of_memory = WSAENOBUFS;
    constexpr int ec_interrupted_syscall = WSAEINTR;
#else
    using native_socket_t = int;
    using setsockopt_ptr = const void*;
    using socket_send_ptr = const void*;
    using socket_recv_ptr = void*;
    constexpr native_socket_t invalid_socket = -1;
    inline void closesocket(native_socket_t fd) { close(fd); }
    inline int last_socket_error() { return errno; }
    inline bool would_block_or_temporarily_unavailable(int errcode) {
        return errcode == EAGAIN || errcode == EWOULDBLOCK;
    }
    constexpr int ec_out_of_memory = ENOMEM;
    constexpr int ec_interrupted_syscall = EINTR;
#endif

/**
 * @brief Platform-specific native socket type.
 */
using native_socket = native_socket_t;

/**
 * @brief Platform-specific native acceptor socket type.
 */
using native_socket_acceptor = native_socket;

inline int64_t int64_from_native_socket(native_socket sock) {
    // on Windows, SOCK is an unsigned value;
    // hence, static_cast<> alone would yield the wrong result,
    // as our io_handle assumes -1 as invalid value
    return sock == invalid_socket ? -1 : static_cast<int64_t>(sock);
}

/**
 * @brief Returns the last socket error as human-readable string.
 */
std::string last_socket_error_as_string();

/**
 * @brief Sets fd to nonblocking if <tt>set_nonblocking == true</tt>
 *        or to blocking if <tt>set_nonblocking == false</tt>
 *        throws @p network_error on error
 */
void nonblocking(native_socket fd, bool new_value);

/**
 * @brief Creates two connected sockets. The former is the read handle
 *        and the latter is the write handle.
 */
std::pair<native_socket, native_socket> create_pipe();

/**
 * @brief Throws network_error with given error message and
 *        the platform-specific error code if @p add_errno is @p true.
 */
void throw_io_failure(const char* what, bool add_errno = true);

/**
 * @brief Returns true if @p fd is configured as nodelay socket.
 * @throws network_error
 */
void tcp_nodelay(native_socket fd, bool new_value);

/**
 * @brief Throws @p network_error if @p result is invalid.
 */
void handle_write_result(ssize_t result);

/**
 * @brief Throws @p network_error if @p result is invalid.
 */
void handle_read_result(ssize_t result);

/**
 * @brief Reads up to @p len bytes from @p fd, writing the received data
 *        to @p buf. Returns @p true as long as @p fd is readable and @p false
 *        if the socket has been closed or an IO error occured. The number
 *        of read bytes is stored in @p result (can be 0).
 */
bool read_some(size_t& result, native_socket fd, void* buf, size_t len);

/**
 * @brief Writes up to @p len bytes from @p buf to @p fd.
 *        Returns @p true as long as @p fd is readable and @p false
 *        if the socket has been closed or an IO error occured. The number
 *        of written bytes is stored in @p result (can be 0).
 */
bool write_some(size_t& result, native_socket fd, const void* buf, size_t len);

/**
 * @brief Tries to accept a new connection from @p fd. On success,
 *        the new connection is stored in @p result. Returns true
 *        as long as
 */
bool try_accept(native_socket& result, native_socket fd);

/**
 * @brief Identifies network IO operations, i.e., read or write.
 */
enum class operation {
    read,
    write,
    propagate_error
};

class multiplexer;

/**
 * @brief A socket IO event handler.
 */
class event_handler {

    friend class multiplexer;

 public:

    event_handler();

    virtual ~event_handler();

    /**
     * @brief Returns true once the requested operation is done, i.e.,
     *        to signalize the multiplexer to remove this handler.
     *        The handler remains in the event loop as long as it returns false.
     */
    virtual void handle_event(operation op) = 0;

    /**
     * @brief Callback to signalize that this handler has been removed
     *        from the event loop for operations of type @p op.
     */
    virtual void removed_from_loop(operation op) = 0;

    /**
     * @brief Returns the bit field storing the subscribed events.
     */
    inline int eventbf() const {
        return m_eventbf;
    }

    /**
     * @brief Sets the bit field storing the subscribed events.
     */
    inline void eventbf(int eventbf) {
        m_eventbf = eventbf;
    }

 protected: // used by the epoll implementation

    virtual native_socket fd() const = 0;

    int m_eventbf;

};

class supervisor;

/**
 * @brief Low-level backend for IO multiplexing.
 */
class multiplexer {

    struct runnable : extend<memory_managed>::with<mixin::memory_cached> {
        virtual void run() = 0;
        virtual ~runnable();
    };

    friend class io::middleman; // disambiguate reference
    friend class supervisor;

 public:

    struct event {
        native_socket fd;
        int mask;
        event_handler* ptr;
    };

    multiplexer();

    ~multiplexer();

    template<typename F>
    void dispatch(F fun, bool force_delayed_execution = false) {
        if (!force_delayed_execution && std::this_thread::get_id() == m_tid) {
            fun();
            return;
        }
        struct impl : runnable {
            F f;
            impl(F&& mf) : f(std::move(mf)) { }
            void run() override {
                f();
            }
        };
        runnable* ptr = detail::memory::create<impl>(std::move(fun));
        wr_dispatch_request(ptr);
    }

    void add(operation op, native_socket fd, event_handler* ptr);

    void del(operation op, native_socket fd, event_handler* ptr);

    void run();

 private:

    template<typename F>
    void new_event(F fun, operation op, native_socket fd, event_handler* ptr) {
        CAF_REQUIRE(fd != invalid_socket);
        CAF_REQUIRE(ptr != nullptr || fd == m_pipe.first);
        // the only valid input where ptr == nullptr is our pipe
        // read handle which is only registered for reading
        auto old_bf = ptr ? ptr->eventbf() : input_mask;
        //auto bf = fun(op, old_bf);
        CAF_LOG_TRACE(CAF_TARG(op, static_cast<int>)
                       << ", " << CAF_ARG(fd) << ", " CAF_ARG(ptr)
                       << ", " << CAF_ARG(old_bf));
        auto event_less = [](const event& e, native_socket fd) -> bool {
            return e.fd < fd;
        };
        auto last = m_events.end();
        auto i = std::lower_bound(m_events.begin(), last, fd, event_less);
        if (i != last && i->fd == fd) {
            CAF_REQUIRE(ptr == i->ptr);
            // squash events together
            CAF_LOG_DEBUG("squash events: " << i->mask << " -> "
                           << fun(op, i->mask));
            auto bf = i->mask;
            i->mask = fun(op, bf);
            if (i->mask == bf) {
                // didn't do a thing
                CAF_LOG_DEBUG("squashing did not change the event");
            } else if (i->mask == old_bf) {
                // just turned into a nop
                CAF_LOG_DEBUG("squashing events resulted in a NOP");
                m_events.erase(i);
            }
        } else {
            // insert new element
            auto bf = fun(op, old_bf);
            if (bf == old_bf) {
                CAF_LOG_DEBUG("event has no effect (discarded): "
                               << CAF_ARG(bf) << ", " << CAF_ARG(old_bf));
            } else {
                m_events.insert(i, event{fd, bf, ptr});
            }
        }
    }

    void handle(const event& event);

    void handle_socket_event(native_socket fd, int mask, event_handler* ptr);

    void close_pipe();

    void wr_dispatch_request(runnable* ptr);

    runnable* rd_dispatch_request();

    native_socket m_epollfd; // unused in poll() implementation
    std::vector<multiplexer_data> m_pollset;
    std::vector<event> m_events; // always sorted by .fd
    multiplexer_poll_shadow_data m_shadow;
    std::pair<native_socket, native_socket> m_pipe;

    std::thread::id m_tid;

};

multiplexer& get_multiplexer_singleton();

/**
 * @brief Makes sure a {@link multiplexer} does not stop its event loop
 *        before the application requests a shutdown.
 *
 * The supervisor informs the multiplexer in its constructor that it
 * must not exit the event loop until the destructor of the supervisor
 * has been called.
 */
class supervisor {

 public:

    supervisor(multiplexer&);

    virtual ~supervisor();

 private:

    multiplexer& m_multiplexer;

};

/**
 * @brief Low-level socket type used as default.
 */
class default_socket {

 public:

    using socket_type = default_socket;

    default_socket(multiplexer& parent, native_socket sock = invalid_socket);

    default_socket(default_socket&& other);

    default_socket& operator=(default_socket&& other);

    ~default_socket();

    void close_read();

    inline native_socket fd() const {
        return m_fd;
    }

    inline native_socket native_handle() const { // ASIO compatible signature
        return m_fd;
    }

    inline multiplexer& backend() {
        return m_parent;
    }

 private:

    multiplexer& m_parent;
    native_socket m_fd;

};

/**
 * @brief Low-level socket type used as default.
 */
using default_socket_acceptor = default_socket;

template<typename T>
inline connection_handle conn_hdl_from_socket(const T& sock) {
    return connection_handle::from_int(
                int64_from_native_socket(sock.native_handle()));
}

template<typename T>
inline accept_handle accept_hdl_from_socket(const T& sock) {
    return accept_handle::from_int(
                int64_from_native_socket(sock.native_handle()));
}


/**
 * @brief A manager configures an IO device and provides callbacks
 *        for various IO operations.
 */
class manager : public ref_counted {

 public:

    virtual ~manager();

    /**
     * @brief Causes the manager to stop read operations on its IO device.
     *        Unwritten bytes are still send before the socket will
     *        be closed.
     */
    virtual void stop_reading() = 0;

    /**
     * @brief Called by the underlying IO device to report failures.
     */
    virtual void io_failure(operation op) = 0;
};

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
class stream : public event_handler {

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

    stream(multiplexer& backend) : m_sock(backend), m_writing(false) {
        configure_read(receive_policy::at_most(1024));
    }

    /**
     * @brief Returns the @p multiplexer this stream belongs to.
     */
    inline multiplexer& backend() {
        return m_sock.backend();
    }

    /**
     * @brief Returns the IO socket.
     */
    inline Socket& socket_handle() {
        return m_sock;
    }

    /**
     * @brief Initializes this stream, setting the socket handle to @p fd.
     */
    void init(Socket fd) {
        m_sock = std::move(fd);
    }

    /**
     * @brief Starts reading data from the socket, forwarding incoming
     *        data to @p mgr.
     */
    void start(const manager_ptr& mgr) {
        CAF_REQUIRE(mgr != nullptr);
        m_reader = mgr;
        backend().add(operation::read, m_sock.fd(), this);
        read_loop();
    }

    void removed_from_loop(operation op) override {
        switch (op) {
            case operation::read:  m_reader.reset(); break;
            case operation::write: m_writer.reset(); break;
            default: break;
        }
    }

    /**
     * @brief Configures how much data will be provided
     *        for the next @p consume callback.
     * @warning Must not be called outside the IO multiplexers event loop
     *          once the stream has been started.
     */
    void configure_read(receive_policy::config config) {
        m_rd_flag = config.first;
        m_max = config.second;
    }

    /**
     * @brief Copies data to the write buffer.
     * @note Not thread safe.
     */
    void write(const void* buf, size_t num_bytes) {
        CAF_LOG_TRACE("num_bytes: " << num_bytes);
        auto first = reinterpret_cast<const char*>(buf);
        auto last  = first + num_bytes;
        m_wr_offline_buf.insert(m_wr_offline_buf.end(), first, last);
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
        CAF_REQUIRE(mgr != nullptr);
        CAF_LOG_TRACE("offline buf size: " << m_wr_offline_buf.size()
                       << ", mgr = " << mgr.get()
                       << ", m_writer = " << m_writer.get());
        if (!m_wr_offline_buf.empty() && !m_writing) {
            backend().add(operation::write, m_sock.fd(), this);
            m_writer = mgr;
            m_writing = true;
            write_loop();
        }
    }

    void stop_reading() {
        CAF_LOGM_TRACE("caf::io::network::stream", "");
        m_sock.close_read();
        backend().del(operation::read, m_sock.fd(), this);
    }

    void handle_event(operation op) {
        CAF_LOG_TRACE("op = " << static_cast<int>(op));
        switch (op) {
            case operation::read: {
                size_t rb; // read bytes
                if (!read_some(rb, m_sock.fd(),
                               m_rd_buf.data() + m_collected,
                               m_rd_buf.size() - m_collected)) {
                    m_reader->io_failure(operation::read);
                    backend().del(operation::read, m_sock.fd(), this);
                } else if (rb > 0) {
                    m_collected += rb;
                    if (m_collected >= m_threshold) {
                        m_reader->consume(m_rd_buf.data(), m_collected);
                        read_loop();
                    }
                }
                break;
            }
            case operation::write: {
                size_t wb; // written bytes
                if (!write_some(wb, m_sock.fd(),
                                m_wr_buf.data() + m_written,
                                m_wr_buf.size() - m_written)) {
                    m_writer->io_failure(operation::write);
                    backend().del(operation::write, m_sock.fd(), this);
                }
                else if (wb > 0) {
                    m_written += wb;
                    if (m_written >= m_wr_buf.size()) {
                        // prepare next send (or stop sending)
                        write_loop();
                    }
                }
                break;
            }
            case operation::propagate_error:
                if (m_reader) {
                    m_reader->io_failure(operation::read);
                }
                if (m_writer) {
                    m_writer->io_failure(operation::write);
                }
                // backend will delete this handler anyway,
                // no need to call backend().del() here
                break;
        }
    }

 protected:

    native_socket fd() const override {
        return m_sock.fd();
    }

 private:

    void read_loop() {
        m_collected = 0;
        switch (m_rd_flag) {
            case receive_policy_flag::exactly:
                if (m_rd_buf.size() != m_max) {
                    m_rd_buf.resize(m_max);
                }
                m_threshold = m_max;
                break;
            case receive_policy_flag::at_most:
                if (m_rd_buf.size() != m_max) {
                    m_rd_buf.resize(m_max);
                }
                m_threshold = 1;
                break;
            case receive_policy_flag::at_least: {
                // read up to 10% more, but at least allow 100 bytes more
                auto max_size =   m_max
                                + std::max<size_t>(100, m_max / 10);
                if (m_rd_buf.size() != max_size) {
                    m_rd_buf.resize(max_size);
                }
                m_threshold = m_max;
                break;
            }
        }
    }

    void write_loop() {
        CAF_LOG_TRACE("wr_buf size: " << m_wr_buf.size()
                       << ", offline buf size: " << m_wr_offline_buf.size());
        m_written = 0;
        m_wr_buf.clear();
        if (m_wr_offline_buf.empty()) {
            m_writing = false;
            backend().del(operation::write, m_sock.fd(), this);
        } else {
            m_wr_buf.swap(m_wr_offline_buf);
        }
    }

    // reading & writing
    Socket              m_sock;

    // reading
    manager_ptr         m_reader;
    size_t              m_threshold;
    size_t              m_collected;
    size_t              m_max;
    receive_policy_flag m_rd_flag;
    buffer_type         m_rd_buf;

    // writing
    manager_ptr         m_writer;
    bool                m_writing;
    size_t              m_written;
    buffer_type         m_wr_buf;
    buffer_type         m_wr_offline_buf;

};

/**
 * @brief An acceptor manager configures an acceptor and provides
 *        callbacks for incoming connections as well as for error handling.
 */
class acceptor_manager : public manager {

 public:

    ~acceptor_manager();

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
class acceptor : public event_handler {

 public:

    using socket_type = typename SocketAcceptor::socket_type;

    /**
     * @brief A manager providing the @p accept member function.
     */
    using manager_type = acceptor_manager;

    /**
     * @brief A smart pointer to an acceptor manager.
     */
    using manager_ptr = intrusive_ptr<manager_type>;

    acceptor(multiplexer& backend)
            : m_backend(backend), m_accept_sock(backend), m_sock(backend) {
        // nop
    }

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
        return m_accept_sock;
    }

    /**
     * @brief Returns the accepted socket. This member function should
     *        be called only from the @p new_connection callback.
     */
    inline socket_type& accepted_socket() {
        return m_sock;
    }

    /**
     * @brief Initializes this acceptor, setting the socket handle to @p fd.
     */
    void init(SocketAcceptor sock) {
        CAF_LOG_TRACE("sock.fd = " << sock.fd());
        m_accept_sock = std::move(sock);
    }

    /**
     * @brief Starts this acceptor, forwarding all incoming connections to
     *        @p manager. The intrusive pointer will be released after the
     *        acceptor has been closed or an IO error occured.
     */
    void start(const manager_ptr& mgr) {
        CAF_LOG_TRACE("m_accept_sock.fd = " << m_accept_sock.fd());
        CAF_REQUIRE(mgr != nullptr);
        m_mgr = mgr;
        backend().add(operation::read, m_accept_sock.fd(), this);
    }

    /**
     * @brief Closes the network connection, thus stopping this acceptor.
     */
    void stop_reading() {
        CAF_LOG_TRACE("m_accept_sock.fd = " << m_accept_sock.fd()
                       << ", m_mgr = " << m_mgr.get());
        backend().del(operation::read, m_accept_sock.fd(), this);
        m_accept_sock.close_read();
    }

    void handle_event(operation op) override {
        CAF_LOG_TRACE("m_accept_sock.fd = " << m_accept_sock.fd()
                       << ", op = " << static_cast<int>(op));
        if (m_mgr && op == operation::read) {
            native_socket fd = invalid_socket;
            if (try_accept(fd, m_accept_sock.fd())) {
                if (fd != invalid_socket) {
                    m_sock = socket_type{backend(), fd};
                    m_mgr->new_connection();
                }
            }
        }
    }

    void removed_from_loop(operation op) override {
        CAF_LOG_TRACE("m_accept_sock.fd = " << m_accept_sock.fd()
                       << "op = " << static_cast<int>(op));
        if (op == operation::read) m_mgr.reset();
    }

 protected:

    native_socket fd() const override {
        return m_accept_sock.fd();
    }

 private:

    multiplexer&   m_backend;
    manager_ptr    m_mgr;
    SocketAcceptor m_accept_sock;
    socket_type    m_sock;

};

native_socket new_ipv4_connection_impl(const std::string&, uint16_t);

default_socket new_ipv4_connection(const std::string& host, uint16_t port);

template<class Socket>
void ipv4_connect(Socket& sock, const std::string& host, uint16_t port) {
    sock = new_ipv4_connection(host, port);
}

native_socket new_ipv4_acceptor_impl(uint16_t port, const char* addr);

default_socket_acceptor new_ipv4_acceptor(uint16_t port,
                                          const char* addr = nullptr);

template<class SocketAcceptor>
void ipv4_bind(SocketAcceptor& sock,
               uint16_t port,
               const char* addr = nullptr) {
    CAF_LOGF_TRACE(CAF_ARG(port));
    sock = new_ipv4_acceptor(port, addr);
}

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_HPP
