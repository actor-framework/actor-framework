#include "cppa/config.hpp"

#include <ios> // ios_base::failure
#include <list>
#include <memory>
#include <iostream>
#include <stdexcept>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

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
#include "cppa/detail/actor_proxy_cache.hpp"

using std::cout;
using std::endl;

using cppa::detail::get_actor_proxy_cache;

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

struct mailman_send_job
{
    process_information_ptr target_peer;
    message original_message;
    mailman_send_job(actor_proxy_ptr apptr, message msg)
        : target_peer(apptr->parent_process_ptr()), original_message(msg)
    {
    }
    mailman_send_job(process_information_ptr peer, message msg)
        : target_peer(peer), original_message(msg)
    {
    }
};

struct mailman_add_peer
{
    native_socket_t sockfd;
    process_information_ptr pinfo;
    mailman_add_peer(native_socket_t sfd, const process_information_ptr& pinf)
        : sockfd(sfd), pinfo(pinf)
    {
    }
};

class mailman_job
{

    friend class
    util::single_reader_queue<mailman_job>;

 public:

    enum job_type
    {
        send_job_type,
        add_peer_type,
        kill_type
    };

 private:

    mailman_job* next;
    job_type m_type;
    union
    {
        mailman_send_job m_send_job;
        mailman_add_peer m_add_socket;
    };

    mailman_job(job_type jt) : next(0), m_type(jt)
    {
    }

 public:

    mailman_job(process_information_ptr piptr, const message& omsg)
        : next(0), m_type(send_job_type)
    {
        new (&m_send_job) mailman_send_job(piptr, omsg);
    }

    mailman_job(actor_proxy_ptr apptr, const message& omsg)
        : next(0), m_type(send_job_type)
    {
        new (&m_send_job) mailman_send_job(apptr, omsg);
    }

    mailman_job(native_socket_t sockfd, const process_information_ptr& pinfo)
        : next(0), m_type(add_peer_type)
    {
        new (&m_add_socket) mailman_add_peer(sockfd, pinfo);
    }

    static mailman_job* kill_job()
    {
        return new mailman_job(kill_type);
    }

    ~mailman_job()
    {
        switch (m_type)
        {
         case send_job_type:
            m_send_job.~mailman_send_job();
            break;
         case add_peer_type:
            m_add_socket.~mailman_add_peer();
            break;
        default: break;
        }
    }

    mailman_send_job& send_job()
    {
        return m_send_job;
    }

    mailman_add_peer& add_peer_job()
    {
        return m_add_socket;
    }

    job_type type() const
    {
        return m_type;
    }

    bool is_send_job() const
    {
        return m_type == send_job_type;
    }

    bool is_add_peer_job() const
    {
        return m_type == add_peer_type;
    }

    bool is_kill_job() const
    {
        return m_type == kill_type;
    }

};

void mailman_loop();

//util::single_reader_queue<mailman_job> s_mailman_queue;
//boost::thread s_mailman_thread(mailman_loop);

struct mailman_manager
{

    boost::thread* m_loop;
    util::single_reader_queue<mailman_job>* m_queue;

    mailman_manager()
    {
        m_queue = new util::single_reader_queue<mailman_job>;
        m_loop = new boost::thread(mailman_loop);
    }

    ~mailman_manager()
    {
        m_queue->push_back(mailman_job::kill_job());
        m_loop->join();
        delete m_loop;
        delete m_queue;
    }

}
s_mailman_manager;

util::single_reader_queue<mailman_job>& s_mailman_queue()
{
    return *(s_mailman_manager.m_queue);
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
                        atom(":KillProxy"),
                        exit_reason::remote_link_unreachable);
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
        s_mailman_queue().push_back(new mailman_job(peer, msg));
    }

};

template<typename T>
T& operator<<(T& o, const process_information& pinfo)
{
    return (o << pinfo.process_id << "@" << pinfo.node_id_as_string());
}

// handles *all* outgoing messages
void mailman_loop()
{
    //cout << "mailman_loop()" << endl;
    //link_map links;
    int flags = 0;
    binary_serializer bs;
    mailman_job* job = nullptr;
    //const auto& pself = process_information::get();
    std::map<process_information, native_socket_t> peers;
    for (;;)
    {
        job = s_mailman_queue().pop();
        if (job->is_send_job())
        {
            mailman_send_job& sjob = job->send_job();
            const message& out_msg = sjob.original_message;
            // forward message to receiver peer
            auto peer_element = peers.find(*(sjob.target_peer));
            if (peer_element != peers.end())
            {
                auto peer = peer_element->second;
                try
                {
                    bs << out_msg;
                    auto size32 = static_cast<std::uint32_t>(bs.size());
                    //cout << "--> " << (to_string(out_msg) + "\n");
                    auto sent = ::send(peer, &size32, sizeof(size32), flags);
                    if (sent != -1)
                    {
                        sent = ::send(peer, bs.data(), bs.size(), flags);
                    }
                    if (sent == -1)
                    {
                        // peer unreachable
                        //cout << "peer " << *(sjob.target_peer)
                        //     << " unreachable" << endl;
                        peers.erase(*(sjob.target_peer));
                    }
                }
                catch (...)
                {
                    // TODO: some kind of error handling?
                }
                bs.reset();
            }
            else
            {
                // TODO: some kind of error handling?
            }
        }
        else if (job->is_add_peer_job())
        {
            mailman_add_peer& pjob = job->add_peer_job();
            auto i = peers.find(*(pjob.pinfo));
            if (i == peers.end())
            {
                //cout << "mailman added " << pjob.pinfo->process_id << "@"
                //     << pjob.pinfo->node_id_as_string() << endl;
                peers.insert(std::make_pair(*(pjob.pinfo), pjob.sockfd));
            }
            else
            {
                // TODO: some kind of error handling?
            }
        }
        else if (job->is_kill_job())
        {
            delete job;
            return;
        }
        delete job;
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

// handles *one* socket / peer
void post_office_loop(native_socket_t socket_fd,
                      process_information_ptr peer,
                      actor_proxy_ptr aptr)
{
    //cout << "--> post_office_loop; self() = "
    //     << process_information::get()
    //     << ", peer = "
    //     << *peer
    //     << endl;
    if (aptr) detail::get_actor_proxy_cache().add(aptr);
    message msg;
    std::uint32_t rsize;
    char* buf = nullptr;
    size_t buf_size = 0;
    size_t buf_allocated = 0;
    auto meta_msg = uniform_typeid<message>();
    const std::type_info& atom_tinfo = typeid(atom_value);
    auto& pinfo = process_information::get();
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
            //cout << "<-- " << (to_string(msg) + "\n");
            if (   msg.content().size() == 1
                && msg.content().utype_info_at(0) == atom_tinfo
                && *reinterpret_cast<const atom_value*>(msg.content().at(0))
                   == atom(":Monitor"))
            {
                actor_ptr sender = msg.sender();
                if (sender->parent_process() == pinfo)
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
    catch (std::ios_base::failure& e)
    {
        //cout << "std::ios_base::failure: " << e.what() << endl;
    }
    catch (std::exception& e)
    {
        //cout << "[" << process_information::get() << "] "
        //     << detail::to_uniform_name(typeid(e)) << ": "
        //     << e.what() << endl;
    }
    //cout << "[" << process_information::get() << "] ~post_office_loop"
    //     << endl;
}

struct mm_worker
{

    native_socket_t m_sockfd;
    boost::thread m_thread;

    mm_worker(native_socket_t sockfd, process_information_ptr peer)
        : m_sockfd(sockfd), m_thread(post_office_loop,
                                     sockfd,
                                     peer,
                                     actor_proxy_ptr())
    {
    }

    ~mm_worker()
    {
        closesocket(m_sockfd);
        m_thread.join();
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
        closesocket(m_sockfd);
        if (m_barrier) m_barrier->wait();
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
            s_mailman_queue().push_back(new mailman_job(sockfd, peer));
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
    s_mailman_queue().push_back(new mailman_job(piptr, msg));
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
    s_mailman_queue().push_back(new mailman_job(sockfd, pinfptr));
    boost::thread(post_office_loop, sockfd, peer_pinf, result).detach();
    return result;
}

} // namespace cppa
