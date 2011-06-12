#include "cppa/config.hpp"

#include <ios> // ios_base::failure
#include <list>
#include <memory>
#include <iostream>
#include <stdexcept>

#include <boost/thread.hpp>

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
#include "cppa/detail/native_socket.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"

using std::cout;
using std::endl;

using cppa::detail::mailman_job;
using cppa::detail::mailman_queue;
using cppa::detail::native_socket_t;
using cppa::detail::get_actor_proxy_cache;

namespace cppa {

namespace {

// a map that manages links between local actors and remote actors (proxies)
typedef std::map<actor_ptr, std::list<actor_proxy_ptr> > link_map;

class remote_observer : public attachable
{

    process_information_ptr peer;

 public:

    remote_observer(const process_information_ptr& piptr) : peer(piptr)
    {
    }

    void detach(std::uint32_t reason)
    {
        actor_ptr self_ptr = self();
        message msg(self_ptr, self_ptr, atom(":KillProxy"), reason);
        detail::mailman_queue().push_back(new detail::mailman_job(peer, msg));
    }

};

template<typename T>
T& operator<<(T& o, const process_information& pinfo)
{
    return (o << pinfo.process_id << "@" << pinfo.node_id_as_string());
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

// handles *one* socket / peer
void post_office_loop(native_socket_t socket_fd,
                      process_information_ptr peer,
                      actor_proxy_ptr aptr,
                      attachable* attachable_ptr)
{
    //cout << "--> post_office_loop; self() = "
    //     << process_information::get()
    //     << ", peer = "
    //     << *peer
    //     << endl;
    // destroys attachable_ptr if the function scope is leaved
    std::unique_ptr<attachable> exit_guard(attachable_ptr);
    if (aptr) detail::get_actor_proxy_cache().add(aptr);
    message msg;
    std::uint32_t rsize;
    char* buf = nullptr;
    size_t buf_size = 0;
    size_t buf_allocated = 0;
    auto meta_msg = uniform_typeid<message>();
    const std::type_info& atom_tinfo = typeid(atom_value);
    auto& pself = process_information::get();
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
            //cout << "[" << pinfo << "] read " << rsize << " bytes" << endl;
            read_from_socket(socket_fd, buf, buf_size);
            binary_deserializer bd(buf, buf_size);
            meta_msg->deserialize(&msg, &bd);
cout << pself.process_id << " <-- " << (to_string(msg) + "\n");
            if (   msg.content().size() == 1
                && msg.content().utype_info_at(0) == atom_tinfo
                && *reinterpret_cast<const atom_value*>(msg.content().at(0))
                   == atom(":Monitor"))
            {
                actor_ptr sender = msg.sender();
                if (sender->parent_process() == pself)
                {
                    //cout << pinfo << " ':Monitor'; actor id = "
                    //     << sender->id() << endl;
                    // local actor?
                    // this message was send from a proxy
                    sender->attach(new remote_observer(peer));
                }
            }
            else
            {
                auto r = msg.receiver();
                if (r) r->enqueue(msg);
            }
        }
    }
    catch (std::exception& e)
    {
        cout << "[" << process_information::get() << "] "
             << detail::to_uniform_name(typeid(e)) << ": "
             << e.what() << endl;
    }
    cout << "kill " << detail::actor_proxy_cache().size() << " proxies" << endl;
    detail::actor_proxy_cache().for_each([](actor_proxy_ptr& pptr)
    {
        cout << "send :KillProxy message" << endl;
        if (pptr) pptr->enqueue(message(nullptr, pptr, atom(":KillProxy"),
                                        exit_reason::remote_link_unreachable));
    });
    cout << "[" << process_information::get() << "] ~post_office_loop"
         << endl;
}

struct mm_worker
{

    native_socket_t m_sockfd;
    boost::thread m_thread;

    mm_worker(native_socket_t sockfd, process_information_ptr peer)
        : m_sockfd(sockfd), m_thread(post_office_loop,
                                     sockfd,
                                     peer,
                                     actor_proxy_ptr(),
                                     static_cast<attachable*>(nullptr))
    {
    }

    ~mm_worker()
    {
cout << "=> [" << process_information::get() << "]::~mm_worker()" << endl;
        detail::closesocket(m_sockfd);
        m_thread.join();
cout << "<= [" << process_information::get() << "]::~mm_worker()" << endl;
    }

};

struct shared_barrier : ref_counted
{
    boost::barrier m_barrier;
    shared_barrier() : m_barrier(2) { }
    ~shared_barrier() { }
    void wait() { m_barrier.wait(); }
};

struct mm_handle : attachable
{
    native_socket_t m_sockfd;
    intrusive_ptr<shared_barrier> m_barrier;

    mm_handle(native_socket_t sockfd,
              const intrusive_ptr<shared_barrier>& mbarrier)
        : m_sockfd(sockfd), m_barrier(mbarrier)
    {
    }

    virtual ~mm_handle()
    {
        //cout << "--> ~mm_handle()" << endl;
cout << "=> [" << process_information::get() << "]::~mm_worker()" << endl;
        detail::closesocket(m_sockfd);
        if (m_barrier) m_barrier->wait();
cout << "<= [" << process_information::get() << "]::~mm_worker()" << endl;
        //cout << "<-- ~mm_handle()" << endl;
    }
};

void middle_man_loop(native_socket_t server_socket_fd,
                     const actor_ptr& published_actor,
                     intrusive_ptr<shared_barrier> barrier)
{
    if (!published_actor) return;
    sockaddr addr;
    socklen_t addrlen;
    typedef std::unique_ptr<mm_worker> child_ptr;
    std::vector<child_ptr> children;
    auto id = published_actor->id();
    const process_information& pinf = process_information::get();
    try
    {
        for (;;)
        {
            auto sockfd = accept(server_socket_fd, &addr, &addrlen);
            if (sockfd < 0)
            {
                throw std::ios_base::failure("invalid socket accepted");
            }
            //cout << "socket accepted" << endl;
            ::send(sockfd, &id, sizeof(id), 0);
            ::send(sockfd, &(pinf.process_id), sizeof(pinf.process_id), 0);
            ::send(sockfd, pinf.node_id.data(), pinf.node_id.size(), 0);
            process_information_ptr peer(new process_information);
            read_from_socket(sockfd,
                             &(peer->process_id),
                             sizeof(std::uint32_t));
            read_from_socket(sockfd,
                             peer->node_id.data(),
                             process_information::node_id_size);
            mailman_queue().push_back(new mailman_job(sockfd, peer));
            // todo: check if connected peer is compatible
            children.push_back(child_ptr(new mm_worker(sockfd, peer)));
            //cout << "client connection done" << endl;
        }
    }
    catch (...)
    {
    }
    // calls destructors of all children
    children.clear();
    // wait for handshake
    barrier->wait();
    //cout << "middle_man_loop finished\n";
}

} // namespace <anonmyous>

void actor_proxy::forward_message(const process_information_ptr& piptr,
                                  const message& msg)
{
    mailman_queue().push_back(new mailman_job(piptr, msg));
}

void publish(actor_ptr& whom, std::uint16_t port)
{
    if (!whom) return;
    native_socket_t sockfd;
    struct sockaddr_in serv_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        throw network_exception("could not create server socket");
    }
    memset((char*) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
        throw bind_failure(errno);
    }
    if (listen(sockfd, 10) != 0)
    {
        throw network_exception("listen() failed");
    }
    intrusive_ptr<shared_barrier> barrier_ptr(new shared_barrier);
    boost::thread(middle_man_loop, sockfd, whom, barrier_ptr).detach();
    whom->attach(new mm_handle(sockfd, barrier_ptr));
}

void publish(actor_ptr&& whom, std::uint16_t port)
{
    publish(static_cast<actor_ptr&>(whom), port);
}

actor_ptr remote_actor(const char* host, std::uint16_t port)
{
    native_socket_t sockfd;
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
    auto& pinf = process_information::get();
    ::send(sockfd, &(pinf.process_id), sizeof(pinf.process_id), 0);
    ::send(sockfd, pinf.node_id.data(), pinf.node_id.size(), 0);
    auto peer_pinf = new process_information;
    std::uint32_t remote_actor_id;
    read_from_socket(sockfd, &remote_actor_id, sizeof(remote_actor_id));
    read_from_socket(sockfd,
                     &(peer_pinf->process_id),
                     sizeof(std::uint32_t));
    read_from_socket(sockfd,
                     peer_pinf->node_id.data(),
                     peer_pinf->node_id.size());
    process_information_ptr pinfptr(peer_pinf);
    actor_proxy_ptr result(new actor_proxy(remote_actor_id, pinfptr));
    mailman_queue().push_back(new mailman_job(sockfd, pinfptr));
    auto ptr = get_scheduler()->register_hidden_context();
    boost::thread(post_office_loop, sockfd,
                  peer_pinf, result, ptr.release()).detach();
    return result;
}

} // namespace cppa
