#include "cppa/detail/post_office_msg.hpp"

namespace cppa { namespace detail {

post_office_msg::add_peer::add_peer(native_socket_t peer_socket,
                                    const process_information_ptr& peer_ptr,
                                    const actor_proxy_ptr& peer_actor_ptr,
                                    std::unique_ptr<attachable>&& peer_observer)
    : sockfd(peer_socket)
    , peer(peer_ptr)
    , first_peer_actor(peer_actor_ptr)
    , attachable_ptr(std::move(peer_observer))
{
}

post_office_msg::add_server_socket::add_server_socket(native_socket_t ssockfd,
                                                      const actor_ptr& whom)
    : server_sockfd(ssockfd)
    , published_actor(whom)
{
}

post_office_msg::post_office_msg(native_socket_t arg0,
                                 const process_information_ptr& arg1,
                                 const actor_proxy_ptr& arg2,
                                 std::unique_ptr<attachable>&& arg3)
    : next(nullptr)
    , m_is_add_peer_msg(true)
{
    new (&m_add_peer_msg) add_peer(arg0, arg1, arg2, std::move(arg3));
}

post_office_msg::post_office_msg(native_socket_t arg0, const actor_ptr& arg1)
    : next(nullptr)
    , m_is_add_peer_msg(false)
{
    new (&m_add_server_socket) add_server_socket(arg0, arg1);
}

post_office_msg::~post_office_msg()
{
    if (m_is_add_peer_msg)
    {
        m_add_peer_msg.~add_peer();
    }
    else
    {
        m_add_server_socket.~add_server_socket();
    }
}

} } // namespace cppa::detail
