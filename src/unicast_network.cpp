#include "cppa/config.hpp"

#include <ios> // ios_base::failure
#include <list>
#include <memory>
#include <iostream>
#include <stdexcept>

#include <fcntl.h>
//#include <boost/thread.hpp>

#include "cppa/cppa.hpp"
#include "cppa/atom.hpp"
#include "cppa/match.hpp"
#include "cppa/to_string.hpp"
#include "cppa/exception.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/util/single_reader_queue.hpp"

#include "cppa/detail/mailman.hpp"
#include "cppa/detail/post_office.hpp"
#include "cppa/detail/native_socket.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"

using std::cout;
using std::endl;

namespace cppa {

namespace {

void read_from_socket(detail::native_socket_t sfd, void* buf, size_t buf_size)
{
    char* cbuf = reinterpret_cast<char*>(buf);
    size_t read_bytes = 0;
    size_t left = buf_size;
    int rres = 0;
    size_t urres = 0;
    do
    {
        rres = ::recv(sfd, cbuf + read_bytes, left, 0);
        if (rres <= 0)
        {
            throw std::ios_base::failure("cannot read from closed socket");
        }
        urres = static_cast<size_t>(rres);
        read_bytes += urres;
        left -= urres;
    }
    while (urres < left);
}

} // namespace <anonmyous>

struct socket_guard
{

    bool m_released;
    detail::native_socket_t m_socket;

 public:

    socket_guard(detail::native_socket_t sfd) : m_released(false), m_socket(sfd)
    {
    }

    ~socket_guard()
    {
        if (!m_released) detail::closesocket(m_socket);
    }

    void release()
    {
        m_released = true;
    }

};

void publish(actor_ptr& whom, std::uint16_t port)
{
    if (!whom) return;
    detail::native_socket_t sockfd;
    struct sockaddr_in serv_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        throw network_exception("could not create server socket");
    }
    // closes the socket if an exception occurs
    socket_guard sguard(sockfd);
    memset((char*) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1)
    {
        throw network_exception("unable to get socket flags");
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        throw network_exception("unable to set socket to nonblocking");
    }
    if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
        throw bind_failure(errno);
    }
    if (listen(sockfd, 10) != 0)
    {
        throw network_exception("listen() failed");
    }
    // ok, no exceptions
    sguard.release();
    detail::post_office_publish(sockfd, whom);
    //intrusive_ptr<shared_barrier> barrier_ptr(new shared_barrier);
    //boost::thread(middle_man_loop, sockfd, whom, barrier_ptr).detach();
    //whom->attach(new mm_handle(sockfd, barrier_ptr));
}

void publish(actor_ptr&& whom, std::uint16_t port)
{
    publish(static_cast<actor_ptr&>(whom), port);
}

actor_ptr remote_actor(const char* host, std::uint16_t port)
{
    detail::native_socket_t sockfd;
    struct sockaddr_in serv_addr;
    struct hostent* server;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        throw network_exception("socket creation failed");
    }
    server = gethostbyname(host);
    if (!server)
    {
        std::string errstr = "no such host: ";
        errstr += host;
        throw network_exception(std::move(errstr));
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memmove(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(sockfd, (const sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
        throw network_exception("could not connect to host");
    }
    auto pinf = process_information::get();
    std::uint32_t process_id = pinf->process_id();
    ::send(sockfd, &process_id, sizeof(std::uint32_t), 0);
    ::send(sockfd, pinf->node_id().data(), pinf->node_id().size(), 0);
    std::uint32_t remote_actor_id;
    std::uint32_t peer_pid;
    process_information::node_id_type peer_node_id;
    read_from_socket(sockfd, &remote_actor_id, sizeof(remote_actor_id));
    read_from_socket(sockfd, &peer_pid, sizeof(std::uint32_t));
    read_from_socket(sockfd, peer_node_id.data(), peer_node_id.size());
    auto peer_pinf = new process_information(peer_pid, peer_node_id);
    process_information_ptr pinfptr(peer_pinf);
    actor_proxy_ptr result(new actor_proxy(remote_actor_id, pinfptr));
    detail::mailman_queue().push_back(new detail::mailman_job(sockfd, pinfptr));
    detail::post_office_add_peer(sockfd, pinfptr, result,
                                 std::unique_ptr<attachable>());
    //auto ptr = get_scheduler()->register_hidden_context();
    //boost::thread(post_office_loop, sockfd,
    //              peer_pinf, result, ptr.release()).detach();
    return result;
}

} // namespace cppa
