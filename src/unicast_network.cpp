#include "cppa/config.hpp"

#include <ios> // ios_base::failure
#include <list>
#include <memory>
#include <stdexcept>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <boost/thread.hpp>

#include "cppa/cppa.hpp"
#include "cppa/match.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"
#include "cppa/util/single_reader_queue.hpp"

namespace cppa {

namespace {

#ifdef ACEDIA_WINDOWS
    typedef SOCKET native_socket_t;
    typedef const char* socket_send_ptr;
    typedef char* socket_recv_ptr;
#else
    typedef int native_socket_t;
    typedef const void* socket_send_ptr;
    typedef void* socket_recv_ptr;
    inline void closesocket(native_socket_t s) { close(s); }
#endif

typedef intrusive_ptr<process_information> pinfo_ptr;

class actor_proxy : public actor
{

    pinfo_ptr m_parent;

 public:

    actor_proxy(const pinfo_ptr& parent) : m_parent(parent)
    {
        if (!m_parent) throw std::runtime_error("parent == nullptr");
    }

    actor_proxy(pinfo_ptr&& parent) : m_parent(std::move(parent))
    {
        if (!m_parent) throw std::runtime_error("parent == nullptr");
    }

    void enqueue(const message& msg);

    void join(group_ptr& what);

    void leave(const group_ptr& what);

    void link(intrusive_ptr<actor>& other);

    void unlink(intrusive_ptr<actor>& other);

    bool remove_backlink(const intrusive_ptr<actor>& to);

    bool establish_backlink(const intrusive_ptr<actor>& to);

    const process_information& parent_process() const
    {
        return *m_parent;
    }

};

typedef intrusive_ptr<actor_proxy> actor_proxy_ptr;

struct mailman_job
{
    mailman_job* next;
    actor_proxy_ptr client;
    message original_message;
    mailman_job(actor_proxy_ptr apptr, message omsg)
        : next(0), client(apptr), original_message(omsg)
    {
    }
};

util::single_reader_queue<mailman_job> s_mailman_queue;

void actor_proxy::enqueue(const message& msg)
{
    s_mailman_queue.push_back(new mailman_job(this, msg));
}

void actor_proxy::join(group_ptr& what)
{
}

void actor_proxy::leave(const group_ptr& what)
{
}

void actor_proxy::link(intrusive_ptr<actor>& other)
{
}

void actor_proxy::unlink(intrusive_ptr<actor>& other)
{
}

bool actor_proxy::remove_backlink(const intrusive_ptr<actor>& to)
{
}

bool actor_proxy::establish_backlink(const intrusive_ptr<actor>& to)
{
}

// a map that manages links between local actors and remote actors (proxies)
typedef std::map<actor_ptr, std::list<actor_proxy_ptr> > link_map;

void fake_exits_from_disconnected_links(link_map& links)
{
    for (auto& element : links)
    {
        auto local_actor = element.first;
        auto& remote_actors = element.second;
        for (auto& rem_actor : remote_actors)
        {
            message msg(rem_actor, local_actor,
                        exit_signal(exit_reason::remote_link_unreachable));
            local_actor->enqueue(msg);
        }
    }
}

void add_link(link_map& links,
              const actor_ptr& local_actor,
              const actor_proxy_ptr& remote_actor)
{
    if (local_actor && remote_actor)
    {
        links[local_actor].push_back(remote_actor);
    }
}

void remove_link(link_map& links,
                 const actor_ptr& local_actor,
                 const actor_proxy_ptr& remote_actor)
{
    if (local_actor && remote_actor)
    {
        auto& link_list = links[local_actor];
        link_list.remove(remote_actor);
        if (link_list.empty()) links.erase(local_actor);
    }
}

// handles *all* outgoing messages
void mailman_loop()
{
    link_map links;
    int send_flags = 0;
    binary_serializer bs;
    mailman_job* job = nullptr;
    const auto& pself = process_information::get();
    std::map<process_information, native_socket_t> peers;
    for (;;)
    {
        job = s_mailman_queue.pop();
        // keep track about link states of local actors
        // (remove link states between local and remote actors if needed)
        if (match<exit_signal>(job->original_message.content()))
        {
            auto sender = job->original_message.sender();
            if (pself == sender->parent_process())
            {
                // an exit message from a local actor to a remote actor
                job->client->unlink(sender);
            }
            else
            {
                // an exit message from a remote actor to another remote actor
            }
        }
        // forward message to receiver peer
        auto peer_element = peers.find(job->client->parent_process());
        if (peer_element != peers.end())
        {
            auto peer = peer_element->second;
            try
            {
                bs << job->original_message;
                auto size32 = static_cast<std::uint32_t>(bs.size());
                auto sent = ::send(peer, sizeof(size32), &size32, send_flags);
                if (sent != -1)
                {
                    sent = ::send(peer, bs.data(), bs.size(), send_flags);
                }
                if (sent == -1)
                {
                    // peer unreachable
                    peers.erase(job->client->parent_process());
                }
            }
            catch (...)
            {
                // TODO: some kind of error handling?
            }
            bs.reset();
        }
    }
}

void read_from_socket(native_socket_t sfd, void* buf, size_t buf_size)
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

// handles *one* socket
void post_office_loop(native_socket_t socket_fd) // socket file descriptor
{
    auto meta_msg = uniform_typeid<message>();
    message msg;
    int read_flags = 0;
    std::uint32_t rsize;
    char* buf = nullptr;
    size_t buf_size = 0;
    size_t buf_allocated = 0;
    try
    {
        for (;;)
        {
            read_from_socket(socket_fd, &rsize, sizeof(rsize));
            if (buf_allocated < rsize)
            {
                // release old memory
                delete[] buf;
                // always allocate 1KB chunks
                buf_allocated = 1024;
                while (buf_allocated <= rsize)
                {
                    buf_allocated += 1024;
                }
                buf = new char[buf_allocated];
            }
            buf_size = rsize;
            read_from_socket(socket_fd, buf, buf_size);
            binary_deserializer bd(buf, buf_size);
            uniform_type_info* meta_msg;
            meta_msg->deserialize(&msg, &bd);
            auto r = msg.receiver();
            if (r) r->enqueue(msg);
        }
    }
    catch (std::ios_base::failure& e)
    {
    }
    catch (std::exception& e)
    {
    }
}

void middle_man_loop(native_socket_t server_socket_fd)
{
    sockaddr addr;
    socklen_t addrlen;
    std::vector<boost::thread> children;
    try
    {
        for (;;)
        {
            auto sockfd = accept(server_socket_fd, &addr, &addrlen);
            if (sockfd < 0)
            {
                throw std::ios_base::failure("invalid socket accepted");
            }
            // todo: check if connected peer is compatible
            children.push_back(boost::thread(post_office_loop, sockfd));
        }
    }
    catch (...)
    {
    }
    for (auto& child : children)
    {
        child.join();
    }
}

} // namespace <anonmyous>

void publish(const actor_ptr& whom, std::uint16_t port)
{
    native_socket_t sockfd;
    struct sockaddr_in serv_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        throw std::ios_base::failure("could not create server socket");
    }
    memset((char*) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
        throw std::ios_base::failure("could not bind socket to port");
    }
    if (listen(sockfd, 10) != 0)
    {
        throw std::ios_base::failure("listen() failed");
    }
    boost::thread(middle_man_loop, sockfd).detach();
}

actor_ptr remote_actor(const char* host, std::uint16_t port)
{

}

} // namespace cppa
