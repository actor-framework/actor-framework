#ifndef POST_OFFICE_MSG_HPP
#define POST_OFFICE_MSG_HPP

#include "cppa/attachable.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/process_information.hpp"
#include "cppa/detail/native_socket.hpp"
#include "cppa/util/single_reader_queue.hpp"

namespace cppa { namespace detail {

class post_office_msg
{

    friend class util::single_reader_queue<post_office_msg>;

 public:

    enum msg_type
    {
        add_peer_type,
        add_server_socket_type,
        proxy_exited_type
    };

    struct add_peer
    {

        native_socket_t sockfd;
        process_information_ptr peer;
        actor_proxy_ptr first_peer_actor;
        std::unique_ptr<attachable> attachable_ptr;

        add_peer(native_socket_t peer_socket,
                 const process_information_ptr& peer_ptr,
                 const actor_proxy_ptr& peer_actor_ptr,
                 std::unique_ptr<attachable>&& peer_observer);

    };

    struct add_server_socket
    {

        native_socket_t server_sockfd;
        actor_ptr published_actor;

        add_server_socket(native_socket_t ssockfd, const actor_ptr& whom);

    };

    struct proxy_exited
    {
        actor_proxy_ptr proxy_ptr;
        inline proxy_exited(const actor_proxy_ptr& who) : proxy_ptr(who) { }
    };

    post_office_msg(native_socket_t arg0,
                    const process_information_ptr& arg1,
                    const actor_proxy_ptr& arg2,
                    std::unique_ptr<attachable>&& arg3);

    post_office_msg(native_socket_t arg0, const actor_ptr& arg1);

    post_office_msg(const actor_proxy_ptr& proxy_ptr);

    inline bool is_add_peer_msg() const
    {
        return m_type == add_peer_type;
    }

    inline bool is_add_server_socket_msg() const
    {
        return m_type == add_server_socket_type;
    }

    inline bool is_proxy_exited_msg() const
    {
        return m_type == proxy_exited_type;
    }

    inline add_peer& as_add_peer_msg()
    {
        return m_add_peer_msg;
    }

    inline add_server_socket& as_add_server_socket_msg()
    {
        return m_add_server_socket;
    }

    inline proxy_exited& as_proxy_exited_msg()
    {
        return m_proxy_exited;
    }

    ~post_office_msg();

 private:

    post_office_msg* next;

    msg_type m_type;

    union
    {
        add_peer m_add_peer_msg;
        add_server_socket m_add_server_socket;
        proxy_exited m_proxy_exited;
    };

};

constexpr std::uint32_t rd_queue_event = 0x00;
constexpr std::uint32_t unpublish_actor_event = 0x01;
constexpr std::uint32_t close_socket_event = 0x02;
constexpr std::uint32_t shutdown_event = 0x03;

typedef std::uint32_t pipe_msg[2];
constexpr size_t pipe_msg_size = 2 * sizeof(std::uint32_t);

} } // namespace cppa::detail

#endif // POST_OFFICE_MSG_HPP
