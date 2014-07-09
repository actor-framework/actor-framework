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

#ifndef CPPA_IO_BROKER_HPP
#define CPPA_IO_BROKER_HPP

#include <map>
#include <vector>

#include "cppa/spawn.hpp"
#include "cppa/extend.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/accept_handle.hpp"
#include "cppa/connection_handle.hpp"

#include "cppa/io/network.hpp"

#include "cppa/mixin/functor_based.hpp"
#include "cppa/mixin/behavior_stack_based.hpp"

#include "cppa/policy/not_prioritizing.hpp"
#include "cppa/policy/sequential_invoke.hpp"

namespace cppa {
namespace io {

class broker;
class middleman;

using broker_ptr = intrusive_ptr<broker>;

/**
 * @brief A broker mediates between an actor system
 *        and other components in the network.
 * @extends local_actor
 */
class broker : public extend<local_actor>::
                      with<mixin::behavior_stack_based<behavior>::impl>,
               public spawn_as_is {

    friend class policy::sequential_invoke;

    using super = combined_type;

 public:

    using buffer_type = std::vector<char>;

    class servant {

        friend class broker;

     public:

        virtual ~servant();

     protected:

        virtual void remove_from_broker() = 0;

        virtual message disconnect_message() = 0;

        servant(broker* ptr);

        void set_broker(broker* ptr);

        void disconnect();

        bool m_disconnected;

        broker* m_broker;

    };

    class scribe : public network::stream_manager, public servant {

        using super = servant;

     public:

        ~scribe();

        scribe(broker* parent, connection_handle hdl);

        /**
         * @brief Implicitly starts the read loop on first call.
         */
        virtual void configure_read(receive_policy::config config) = 0;

        /**
         * @brief Grants access to the output buffer.
         */
        virtual buffer_type& wr_buf() = 0;

        /**
         * @brief Flushes the output buffer, i.e., sends the content of
         *        the buffer via the network.
         */
        virtual void flush() = 0;

        inline connection_handle hdl() const { return m_hdl; }

        void io_failure(network::operation op) override;

     protected:

        virtual buffer_type& rd_buf() = 0;

        inline new_data_msg& read_msg() {
            return m_read_msg.get_as_mutable<new_data_msg>(0);
        }

        inline const new_data_msg& read_msg() const {
            return m_read_msg.get_as<new_data_msg>(0);
        }

        void remove_from_broker() override;

        message disconnect_message() override;

        void consume(const void* data, size_t num_bytes) override;

        connection_handle m_hdl;

        message m_read_msg;

    };

    class doorman : public network::acceptor_manager, public servant {

        using super = servant;

     public:

        ~doorman();

        doorman(broker* parent, accept_handle hdl);

        inline accept_handle hdl() const { return m_hdl; }

        void io_failure(network::operation op) override;

        // needs to be launched explicitly
        virtual void launch() = 0;

     protected:

        void remove_from_broker() override;

        message disconnect_message() override;

        inline new_connection_msg& accept_msg() {
            return m_accept_msg.get_as_mutable<new_connection_msg>(0);
        }

        inline const new_connection_msg& accept_msg() const {
            return m_accept_msg.get_as<new_connection_msg>(0);
        }

        accept_handle m_hdl;

        message m_accept_msg;

    };

    class continuation;

    // ... and some helpers need friendship
    friend class scribe;
    friend class doorman;
    friend class continuation;

    ~broker();

    /**
     * @brief Modifies the receive policy for given connection.
     * @param hdl Identifies the affected connection.
     * @param config Contains the new receive policy.
     */
    void configure_read(connection_handle hdl, receive_policy::config config);

    /**
     * @brief Returns the write buffer for given connection.
     */
    buffer_type& wr_buf(connection_handle hdl);

    void write(connection_handle hdl, size_t buf_size, const void* buf);

    /**
     * @brief Sends the content of the buffer for given connection.
     */
    void flush(connection_handle hdl);

    /**
     * @brief Returns the number of open connections.
     */
    inline size_t num_connections() const { return m_scribes.size(); }

    std::vector<connection_handle> connections() const;

    /** @cond PRIVATE */

    template<typename F, typename... Ts>
    actor fork(F fun, connection_handle hdl, Ts&&... vs) {
        // provoke compile-time errors early
        using fun_res = decltype(fun(this, hdl, std::forward<Ts>(vs)...));
        // prevent warning about unused local type
        static_assert(std::is_same<fun_res, fun_res>::value,
                      "your compiler is lying to you");
        auto i = m_scribes.find(hdl);
        if (i == m_scribes.end()) {
            CPPA_LOG_ERROR("invalid handle");
            throw std::invalid_argument("invalid handle");
        }
        auto sptr = i->second;
        CPPA_REQUIRE(sptr->hdl() == hdl);
        m_scribes.erase(i);
        return spawn_functor(nullptr,
                             [sptr](broker* forked) {
                                 sptr->set_broker(forked);
                                 forked->m_scribes.emplace(sptr->hdl(), sptr);
                             },
                             fun, hdl, std::forward<Ts>(vs)...);
    }

    template<class Socket>
    connection_handle add_connection(Socket sock) {
        CPPA_LOG_TRACE("");
        class impl : public scribe {

            using super = scribe;

         public:

            impl(broker* parent, Socket&& s)
                    : super(parent, network::conn_hdl_from_socket(s))
                    , m_launched(false), m_stream(parent->backend()) {
                m_stream.init(std::move(s));
            }

            void configure_read(receive_policy::config config) override {
                CPPA_LOGM_TRACE("boost::actor_io::broker::scribe", "");
                m_stream.configure_read(config);
                if (!m_launched) launch();
            }

            buffer_type& wr_buf() override {
                return m_stream.wr_buf();
            }

            buffer_type& rd_buf() override {
                return m_stream.rd_buf();
            }

            void stop_reading() override {
                CPPA_LOGM_TRACE("boost::actor_io::broker::scribe", "");
                m_stream.stop_reading();
                disconnect();
            }

            void flush() override {
                CPPA_LOGM_TRACE("boost::actor_io::broker::scribe", "");
                m_stream.flush(this);
            }

            void launch() {
                CPPA_LOGM_TRACE("boost::actor_io::broker::scribe", "");
                CPPA_REQUIRE(!m_launched);
                m_launched = true;
                m_stream.start(this);
            }

         private:

            bool m_launched;
            network::stream<Socket> m_stream;

        };
        intrusive_ptr<impl> ptr{new impl{this, std::move(sock)}};
        m_scribes.emplace(ptr->hdl(), ptr);
        return ptr->hdl();
    }

    template<class SocketAcceptor>
    accept_handle add_acceptor(SocketAcceptor sock) {
        CPPA_LOG_TRACE("sock.fd = " << sock.fd());
        CPPA_REQUIRE(sock.fd() != network::invalid_socket);
        class impl : public doorman {

            using super = doorman;

         public:

            impl(broker* parent, SocketAcceptor&& s)
                    : super(parent, network::accept_hdl_from_socket(s))
                    , m_acceptor(parent->backend()) {
                m_acceptor.init(std::move(s));
            }

            void new_connection() override {
                accept_msg().handle = m_broker->add_connection(
                            std::move(m_acceptor.accepted_socket()));
                m_broker->invoke_message(invalid_actor_addr,
                                         message_id::invalid,
                                         m_accept_msg);
            }

            void stop_reading() override {
                m_acceptor.stop_reading();
                disconnect();
            }

            void launch() override {
                m_acceptor.start(this);
            }

            network::acceptor<SocketAcceptor> m_acceptor;

        };
        intrusive_ptr<impl> ptr{new impl{this, std::move(sock)}};
        m_doormen.emplace(ptr->hdl(), ptr);
        if (initialized()) ptr->launch();
        return ptr->hdl();
    }

    void enqueue(const actor_addr&, message_id, message,
                 execution_unit*) override;

    template<typename F>
    static broker_ptr from(F fun) {
        // transform to STD function here, because GCC is unable
        // to select proper overload otherwise ...
        using fres = decltype(fun((broker*)nullptr));
        std::function<fres(broker*)> stdfun{std::move(fun)};
        return from_impl(std::move(stdfun));
    }

    template<typename F, typename T, typename... Ts>
    static broker_ptr from(F fun, T&& v, Ts&&... vs) {
        return from(std::bind(fun, std::placeholders::_1, std::forward<T>(v),
                              std::forward<Ts>(vs)...));
    }

    /**
     * @brief Closes all connections and acceptors.
     */
    void close_all();

    /**
     * @brief Closes the connection identified by @p handle.
     *        Unwritten data will still be send.
     */
    void close(connection_handle handle);

    /**
     * @brief Closes the acceptor identified by @p handle.
     */
    void close(accept_handle handle);

    class functor_based;

    void launch(bool is_hidden, execution_unit*);

    // <backward_compatibility version="0.9">

    static constexpr auto at_least = receive_policy_flag::at_least;

    static constexpr auto at_most = receive_policy_flag::at_most;

    static constexpr auto exactly = receive_policy_flag::exactly;

    void receive_policy(connection_handle hdl,
                        receive_policy_flag flag,
                        size_t num_bytes) CPPA_DEPRECATED {
        configure_read(hdl, receive_policy::config{flag, num_bytes});
    }

    // </backward_compatibility>

 protected:

    broker();

    void cleanup(uint32_t reason) override;

    using scribe_pointer = intrusive_ptr<scribe>;

    using doorman_pointer = intrusive_ptr<doorman>;

    bool initialized() const;

    /** @endcond */

    virtual behavior make_behavior() = 0;

    inline middleman& parent() {
        return m_mm;
    }

    network::multiplexer& backend();

 private:

    template<typename Handle, class T>
    static T& by_id(Handle hdl, std::map<Handle, intrusive_ptr<T>>& elements) {
        auto i = elements.find(hdl);
        if (i == elements.end()) {
            throw std::invalid_argument("invalid handle");
        }
        return *(i->second);
    }

    // throws on error
    inline scribe& by_id(connection_handle hdl) {
        return by_id(hdl, m_scribes);
    }

    // throws on error
    inline doorman& by_id(accept_handle hdl) { return by_id(hdl, m_doormen); }

    void invoke_message(const actor_addr& sender, message_id mid,
                        message& msg);

    bool invoke_message_from_cache();

    void erase_io(int id);

    void erase_acceptor(int id);

    std::map<accept_handle, doorman_pointer> m_doormen;
    std::map<connection_handle, scribe_pointer> m_scribes;

    policy::not_prioritizing m_priority_policy;
    policy::sequential_invoke m_invoke_policy;

    bool m_initialized;

    bool m_hidden;

    bool m_running;

    middleman& m_mm;

};

class broker::functor_based : public extend<broker>::
                                     with<mixin::functor_based> {

    using super = combined_type;

 public:

    template<typename... Ts>
    functor_based(Ts&&... vs)
            : super(std::forward<Ts>(vs)...) {}

    ~functor_based();

    behavior make_behavior() override;

};

} // namespace io
} // namespace cppa

#endif // CPPA_IO_BROKER_HPP
