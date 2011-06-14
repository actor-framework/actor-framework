#include <new>          // placement new
#include <ios>          // ios_base::failure
#include <list>         // std::list
#include <cstdint>      // std::uint32_t
#include <iostream>     // std::cout, std::endl
#include <exception>    // std::logic_error
#include <algorithm>    // std::find_if

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

// used cppa classes
#include "cppa/to_string.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/binary_deserializer.hpp"

// used cppa utility
#include "cppa/util/single_reader_queue.hpp"

// used cppa details
#include "cppa/detail/buffer.hpp"
#include "cppa/detail/mailman.hpp"
#include "cppa/detail/post_office.hpp"
#include "cppa/detail/native_socket.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"

//#define DEBUG(arg) std::cout << arg << std::endl
#define DEBUG(unused) //

namespace cppa { namespace detail { namespace {

// allocate in 1KB chunks (minimize reallocations)
constexpr size_t s_chunk_size = 1024;

// allow up to 1MB per buffer
constexpr size_t s_max_buffer_size = (1024 * 1024);

static_assert((s_max_buffer_size % s_chunk_size) == 0,
              "max_buffer_size is not a multiple of chunk_size");

static_assert(sizeof(native_socket_t) == sizeof(std::uint32_t),
              "sizeof(native_socket_t) != sizeof(std::uint32_t)");

constexpr int s_rdflag = MSG_DONTWAIT;

constexpr std::uint32_t rd_queue_event = 0x00;
constexpr std::uint32_t unpublish_actor_event = 0x01;
constexpr std::uint32_t dec_socket_ref_event = 0x02;
constexpr std::uint32_t close_socket_event = 0x03;
constexpr std::uint32_t shutdown_event = 0x04;

typedef std::uint32_t pipe_msg[2];
constexpr size_t pipe_msg_size = 2 * sizeof(std::uint32_t);

struct add_peer_msg
{

    native_socket_t sockfd;
    process_information_ptr peer;
    actor_proxy_ptr first_peer_actor;
    std::unique_ptr<attachable> attachable_ptr;

    add_peer_msg(native_socket_t peer_socket,
                 const process_information_ptr& peer_ptr,
                 const actor_proxy_ptr& peer_actor_ptr,
                 std::unique_ptr<attachable>&& peer_observer)
        : sockfd(peer_socket)
        , peer(peer_ptr)
        , first_peer_actor(peer_actor_ptr)
        , attachable_ptr(std::move(peer_observer))
    {
    }

};

struct add_server_socket_msg
{

    native_socket_t server_sockfd;
    actor_ptr published_actor;

    add_server_socket_msg(native_socket_t ssockfd,
                          const actor_ptr& pub_actor)
        : server_sockfd(ssockfd)
        , published_actor(pub_actor)
    {
    }

};

class post_office_msg
{

    friend class util::single_reader_queue<post_office_msg>;

    post_office_msg* next;

    bool m_is_add_peer_msg;

    union
    {
        add_peer_msg m_add_peer_msg;
        add_server_socket_msg m_add_server_socket;
    };

 public:

    post_office_msg(native_socket_t arg0,
                    const process_information_ptr& arg1,
                    const actor_proxy_ptr& arg2,
                    std::unique_ptr<attachable>&& arg3)
        : next(nullptr), m_is_add_peer_msg(true)
    {
        new (&m_add_peer_msg) add_peer_msg (arg0, arg1, arg2, std::move(arg3));
    }

    post_office_msg(native_socket_t arg0, const actor_ptr& arg1)
        : next(nullptr), m_is_add_peer_msg(false)
    {
        new (&m_add_server_socket) add_server_socket_msg(arg0, arg1);
    }

    inline bool is_add_peer_msg() const
    {
        return m_is_add_peer_msg;
    }

    inline bool is_add_server_socket_msg() const
    {
        return !m_is_add_peer_msg;
    }

    inline add_peer_msg& as_add_peer_msg()
    {
        return m_add_peer_msg;
    }

    inline add_server_socket_msg& as_add_server_socket_msg()
    {
        return m_add_server_socket;
    }

    ~post_office_msg()
    {
        if (m_is_add_peer_msg)
        {
            m_add_peer_msg.~add_peer_msg();
        }
        else
        {
            m_add_server_socket.~add_server_socket_msg();
        }
    }

};

void post_office_loop(int pipe_read_handle);

// static initialization and destruction
struct post_office_manager
{

    typedef util::single_reader_queue<post_office_msg> queue_t;
    typedef util::single_reader_queue<mailman_job> mailman_queue_t;

    // m_pipe[0] is for reading, m_pipe[1] is for writing
    int m_pipe[2];
    mailman_queue_t m_mailman_queue;
    queue_t m_queue;
    boost::thread* m_loop;

    post_office_manager()
    {
        //if (socketpair(AF_UNIX, SOCK_STREAM, 0, m_pipe) != 0)
        if (pipe(m_pipe) != 0)
        {
            switch (errno)
            {
                case EFAULT:
                {
                    throw std::logic_error("EFAULT: invalid pipe() argument");
                }
                case EMFILE:
                {
                    throw std::logic_error("EMFILE: Too many file "
                                           "descriptors in use");
                }
                case ENFILE:
                {
                    throw std::logic_error("The system limit on the total "
                                           "number of open files "
                                           "has been reached");
                }
                default:
                {
                    throw std::logic_error("unknown error");
                }
            }
        }
        /*
        int flags = fcntl(m_pipe[0], F_GETFL, 0);
        if (flags == -1 || fcntl(m_pipe[0], F_SETFL, flags | O_NONBLOCK) == -1)
        {
            throw std::logic_error("unable to set pipe to nonblocking");
        }
        */
        /*
        int flags = fcntl(m_pipe[0], F_GETFL, 0);
        if (flags == -1 || fcntl(m_pipe[0], F_SETFL, flags | O_ASYNC) == -1)
        {
            throw std::logic_error("unable to set pipe to O_ASYNC");
        }
        */
        m_loop = new boost::thread(post_office_loop, m_pipe[0]);
    }

    int write_handle()
    {
        return m_pipe[1];
    }

    ~post_office_manager()
    {
        DEBUG("~post_office_manager() ...");
        pipe_msg msg = { shutdown_event, 0 };
        write(write_handle(), msg, pipe_msg_size);
        // m_loop calls close(m_pipe[0])
        m_loop->join();
        delete m_loop;
        close(m_pipe[0]);
        close(m_pipe[1]);
        DEBUG("~post_office_manager() ... done");
    }

}
s_po_manager;

} } } // namespace cppa::detail::<anonmyous>

namespace cppa { namespace detail {

util::single_reader_queue<mailman_job>& mailman_queue()
{
    return s_po_manager.m_mailman_queue;
}

} } // namespace cppa::detail

namespace cppa { namespace detail { namespace {

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

void handle_message(const message& msg,
                    const std::type_info& atom_tinfo,
                    const process_information& pself,
                    const process_information_ptr& peer)
{
    if (   msg.content().size() == 1
        && msg.content().utype_info_at(0) == atom_tinfo
        && *reinterpret_cast<const atom_value*>(msg.content().at(0))
           == atom(":Monitor"))
    {
        DEBUG("<-- :Monitor");
        actor_ptr sender = msg.sender();
        if (sender->parent_process() == pself)
        {
            //cout << pinfo << " ':Monitor'; actor id = "
            //     << sender->id() << endl;
            // local actor?
            // this message was send from a proxy
            sender->attach(new remote_observer(peer));
        }
        else
        {
            DEBUG(":Monitor received for an remote actor");
        }
    }
    else
    {
        DEBUG("<-- " << to_string(msg));
        auto r = msg.receiver();
        if (r) r->enqueue(msg);
    }
}

class post_office_worker
{

    size_t m_rc;
    native_socket_t m_parent;

 protected:

    native_socket_t m_socket;

    post_office_worker(native_socket_t fd, native_socket_t parent_fd = -1)
        : m_rc((parent_fd != -1) ? 1 : 0)
        , m_parent(parent_fd)
        , m_socket(fd)
    {
    }

    post_office_worker(post_office_worker&& other)
        : m_rc(other.m_rc)
        , m_parent(other.m_parent)
        , m_socket(other.m_socket)
    {
        other.m_rc = 0;
        other.m_socket = -1;
        other.m_parent = -1;
    }

 public:

    inline size_t ref_count() const
    {
        return m_rc;
    }

    inline void inc_ref_count()
    {
        ++m_rc;
    }

    inline size_t dec_ref_count()
    {
        if (m_rc == 0) throw std::runtime_error("dec_ref_count(): underflow");
        return --m_rc;
    }

    inline native_socket_t get_socket()
    {
        return m_socket;
    }

    inline bool has_parent() const
    {
        return m_parent != -1;
    }

    inline native_socket_t parent() const
    {
        return m_parent;
    }

    // @return new reference count
    size_t parent_exited(native_socket_t parent_socket)
    {
        if (has_parent() && parent() == parent_socket)
        {
            m_parent = -1;
            return dec_ref_count();
        }
        return ref_count();
    }

    virtual ~post_office_worker()
    {
        if (m_socket != -1)
        {
            closesocket(m_socket);
        }
    }

};

class po_peer : public post_office_worker
{

    typedef post_office_worker super;

    enum state
    {
        // connection just established; waiting for process information
        wait_for_process_info,
        // wait for the size of the next message
        wait_for_msg_size,
        // currently reading a message
        read_message
    };

    state m_state;
    process_information_ptr m_peer;
    std::unique_ptr<attachable> m_observer;
    buffer<s_chunk_size, s_max_buffer_size> m_rdbuf;
    std::list<actor_proxy_ptr> m_children;

 public:

    explicit po_peer(add_peer_msg& from)
        : super(from.sockfd)
        , m_state(wait_for_msg_size)
        , m_peer(std::move(from.peer))
        , m_observer(std::move(from.attachable_ptr))
    {
    }

    explicit po_peer(native_socket_t sockfd, native_socket_t parent_socket)
        : super(sockfd, parent_socket)
        , m_state(wait_for_process_info)
    {
        m_rdbuf.reset(  sizeof(std::uint32_t)
                      + process_information::node_id_size);
    }

    po_peer(po_peer&& other)
        : super(std::move(other))
        , m_state(other.m_state)
        , m_peer(std::move(other.m_peer))
        , m_observer(std::move(other.m_observer))
        , m_rdbuf(std::move(other.m_rdbuf))
        , m_children(std::move(other.m_children))
    {
    }

    void add_child(const actor_proxy_ptr& pptr)
    {
        m_children.push_back(pptr);
    }

    ~po_peer()
    {
        if (!m_children.empty())
        {
            for (actor_proxy_ptr& pptr : m_children)
            {
                pptr->enqueue(message(nullptr, nullptr, atom(":KillProxy"),
                                      exit_reason::remote_link_unreachable));
            }
        }
    }

    // @return false if an error occured; otherwise true
    bool read_and_continue(const uniform_type_info* meta_msg,
                           const process_information& pself)
    {
        switch (m_state)
        {
            case wait_for_process_info:
            {
                if (!m_rdbuf.append_from(m_socket, s_rdflag)) return false;
                if (m_rdbuf.ready() == false)
                {
                    break;
                }
                else
                {
                    m_peer.reset(new process_information);
                    // inform mailman about new peer
                    mailman_queue().push_back(new mailman_job(m_socket,
                                                              m_peer));
                    memcpy(&(m_peer->process_id),
                           m_rdbuf.data(),
                           sizeof(std::uint32_t));
                    memcpy(m_peer->node_id.data(),
                           m_rdbuf.data() + sizeof(std::uint32_t),
                           process_information::node_id_size);
                    m_rdbuf.reset();
                    m_state = wait_for_msg_size;
                    DEBUG("pinfo read: "
                          << m_peer->process_id
                          << "@"
                          << m_peer->node_id_as_string());
                    // fall through and try to read more from socket
                }
            }
            case wait_for_msg_size:
            {
                if (m_rdbuf.final_size() != sizeof(std::uint32_t))
                {
                    m_rdbuf.reset(sizeof(std::uint32_t));
                }
                if (!m_rdbuf.append_from(m_socket, s_rdflag)) return false;
                if (m_rdbuf.ready() == false)
                {
                    break;
                }
                else
                {
                    // read and set message size
                    std::uint32_t msg_size;
                    memcpy(&msg_size, m_rdbuf.data(), sizeof(std::uint32_t));
                    m_rdbuf.reset(msg_size);
                    m_state = read_message;
                    // fall through and try to read more from socket
                }
            }
            case read_message:
            {
                if (!m_rdbuf.append_from(m_socket, s_rdflag)) return false;
                if (m_rdbuf.ready())
                {
                    message msg;
                    binary_deserializer bd(m_rdbuf.data(), m_rdbuf.size());
                    try
                    {
                        meta_msg->deserialize(&msg, &bd);
                    }
                    catch (std::exception& e)
                    {
                        DEBUG(to_uniform_name(typeid(e)) << ": " << e.what());
                        return false;
                    }
                    handle_message(msg, typeid(atom_value), pself, m_peer);
                    m_rdbuf.reset();
                    m_state = wait_for_msg_size;
                }
                break;
            }
            default:
            {
                throw std::logic_error("illegal state");
            }
        }
        return true;
    }

};

class po_doorman : public post_office_worker
{

    typedef post_office_worker super;

    // server socket
    actor_ptr published_actor;

 public:

    explicit po_doorman(add_server_socket_msg& assm)
        : super(assm.server_sockfd)
        , published_actor(assm.published_actor)
    {
    }

    po_doorman(po_doorman&& other)
        : super(std::move(other))
        , published_actor(std::move(other.published_actor))
    {
    }

    // @return false if an error occured; otherwise true
    bool read_and_continue(const process_information& pself,
                           std::list<po_peer>& peers)
    {
        sockaddr addr;
        socklen_t addrlen;
        memset(&addr, 0, sizeof(addr));
        memset(&addrlen, 0, sizeof(addrlen));
        auto sfd = ::accept(m_socket, &addr, &addrlen);
        if (sfd < 0)
        {
            switch (errno)
            {
                case EAGAIN:
#               if EAGAIN != EWOULDBLOCK
                case EWOULDBLOCK:
#               endif
                {
                    // just try again
                    return true;
                }
                default: return false;
            }
        }
        auto id = published_actor->id();
        ::send(sfd, &id, sizeof(std::uint32_t), 0);
        ::send(sfd, &(pself.process_id), sizeof(std::uint32_t), 0);
        ::send(sfd, pself.node_id.data(), pself.node_id.size(), 0);
        peers.push_back(po_peer(sfd, m_socket));
        DEBUG("socket accepted; published actor: " << id);
        return true;
    }

};

struct mailman_worker
{

    boost::thread m_thread;

    mailman_worker() : m_thread(mailman_loop)
    {
    }

    ~mailman_worker()
    {
        mailman_queue().push_back(mailman_job::kill_job());
        m_thread.join();
    }

};

void post_office_loop(int pipe_read_handle)
{
    // starts and stops mailman
    mailman_worker mworker;
    // map of all published actors
    std::map<std::uint32_t, std::list<po_doorman> > doormen;
    // list of all connected peers
    std::list<po_peer> peers;
    // readset for select()
    fd_set readset;
    // maximum number of all socket descriptors
    int maxfd = 0;
    // cache some used global data
    auto meta_msg = uniform_typeid<message>();
    auto& pself = process_information::get();
    // initialize variables
    FD_ZERO(&readset);
    maxfd = pipe_read_handle;
    FD_SET(pipe_read_handle, &readset);
    // keeps track about what peer we are iterating at this time
    po_peer* selected_peer = nullptr;
    // thread id of post_office
    auto thread_id = boost::this_thread::get_id();
    // if an actor calls its quit() handler in this thread,
    // we 'catch' the released socket here
    std::vector<native_socket_t> released_socks;
    // functor that releases a socket descriptor
    // returns true if an element was removed from peers
    auto release_socket = [&](native_socket_t sockfd)
    {
        auto end = peers.end();
        auto i = std::find_if(peers.begin(), end, [sockfd](po_peer& pp) -> bool
        {
            return pp.get_socket() == sockfd;
        });
        if (i != end && i->dec_ref_count() == 0) peers.erase(i);
    };
    // initialize proxy cache
    get_actor_proxy_cache().set_callback([&](actor_proxy_ptr& pptr)
    {
        pptr->enqueue(message(pptr, nullptr, atom(":Monitor")));
        if (selected_peer == nullptr)
        {
            throw std::logic_error("selected_peer == nullptr");
        }
        selected_peer->add_child(pptr);
        selected_peer->inc_ref_count();
        auto msock = selected_peer->get_socket();
        pptr->attach_functor([msock, thread_id, &released_socks](std::uint32_t)
        {
            if (boost::this_thread::get_id() == thread_id)
            {
                released_socks.push_back(msock);
            }
            else
            {
                pipe_msg msg = { dec_socket_ref_event,
                                 static_cast<std::uint32_t>(msock) };
                write(s_po_manager.write_handle(), msg, pipe_msg_size);
            }
        });
    });
    for (;;)
    {
//std::cout << __LINE__ << std::endl;
        if (select(maxfd + 1, &readset, nullptr, nullptr, nullptr) < 0)
        {
            // must not happen
            perror("select()");
            exit(3);
        }
//std::cout << __LINE__ << std::endl;
        // iterate over all peers; lifetime scope of i, end
        {
            auto i = peers.begin();
            auto end = peers.end();
            while (i != end)
            {
                if (FD_ISSET(i->get_socket(), &readset))
                {
                    selected_peer = &(*i);
                    //DEBUG("read message from peer");
                    if (i->read_and_continue(meta_msg, pself))
                    {
                        // no errors detected; next iteration
                        ++i;
                    }
                    else
                    {
                        // peer detected an error; erase from list
                        DEBUG("connection to peer lost");
                        i = peers.erase(i);
                    }
                }
                else
                {
                    // next iteration
                    ++i;
                }
            }
        }
        selected_peer = nullptr;
        // new connections to accept?
        for (auto& kvp : doormen)
        {
            auto& list = kvp.second;
            auto i = list.begin();
            auto end = list.end();
            while (i != end)
            {
                if (FD_ISSET(i->get_socket(), &readset))
                {
                    DEBUG("accept new socket...");
                    if (i->read_and_continue(pself, peers))
                    {
                        DEBUG("ok");
                        ++i;
                    }
                    else
                    {
                        DEBUG("failed; erased doorman");
                        i = list.erase(i);
                    }
                }
                else
                {
                    ++i;
                }
            }
        }
        // read events from pipe
        if (FD_ISSET(pipe_read_handle, &readset))
        {
            pipe_msg pmsg;
            //memcpy(pmsg, pipe_msg_buf.data(), pipe_msg_buf.size());
            //pipe_msg_buf.clear();
            ::read(pipe_read_handle, &pmsg, pipe_msg_size);
            switch (pmsg[0])
            {
                case rd_queue_event:
                {
                    DEBUG("rd_queue_event");
                    post_office_msg* pom = s_po_manager.m_queue.pop();
                    if (pom->is_add_peer_msg())
                    {
                        auto& apm = pom->as_add_peer_msg();
                        actor_proxy_ptr pptr = apm.first_peer_actor;
                        po_peer pd(apm);
                        selected_peer = &pd;
                        if (pptr)
                        {
                            DEBUG("proxy added via post_office_msg");
                            get_actor_proxy_cache().add(pptr);
                        }
                        selected_peer = nullptr;
                        peers.push_back(std::move(pd));
                        DEBUG("new peer (remote_actor)");
                    }
                    else
                    {
                        auto& assm = pom->as_add_server_socket_msg();
                        auto& pactor = assm.published_actor;
                        if (!pactor)
                        {
                            throw std::logic_error("nullptr published");
                        }
                        auto actor_id = pactor->id();
                        auto callback = [actor_id](std::uint32_t)
                        {
                            DEBUG("call post_office_unpublish() ...");
                            post_office_unpublish(actor_id);
                        };
                        if (pactor->attach_functor(std::move(callback)))
                        {
                            auto& dm = doormen[actor_id];
                            dm.push_back(po_doorman(assm));
                            DEBUG("new doorman");
                        }
                        // else: actor already exited!
                    }
                    delete pom;
                    break;
                }
                case unpublish_actor_event:
                {
                    DEBUG("unpublish_actor_event");
                    auto kvp = doormen.find(pmsg[1]);
                    if (kvp != doormen.end())
                    {
                        for (po_doorman& dm : kvp->second)
                        {
                            auto i = peers.begin();
                            auto end = peers.end();
                            while (i != end)
                            {
                                if (i->parent_exited(dm.get_socket()) == 0)
                                {
                                    DEBUG("socket closed; parent exited");
                                    i = peers.erase(i);
                                }
                                else
                                {
                                    ++i;
                                }
                            }
                        }
                        doormen.erase(kvp);
                    }
                    break;
                }
                case dec_socket_ref_event:
                {
                    auto sockfd = static_cast<native_socket_t>(pmsg[1]);
                    release_socket(sockfd);
                    break;
                }
                case close_socket_event:
                {
                    auto sockfd = static_cast<native_socket_t>(pmsg[1]);
                    auto i = peers.begin();
                    auto end = peers.end();
                    while (i != end)
                    {
                        if (i->get_socket() == sockfd)
                        {
                            // exit loop
                            peers.erase(i);
                            i = end;
                        }
                        else ++i;
                    }
                    break;
                }
                case shutdown_event:
                {
                    // goodbye
                    return;
                }
                default:
                {
                    std::ostringstream oss;
                    oss << "unexpected event type: " << pmsg[0];
                    throw std::logic_error(oss.str());
                }
            }
        }
        if (released_socks.empty() == false)
        {
            for (native_socket_t sockfd : released_socks)
            {
                release_socket(sockfd);
            }
        }
        // recalculate readset if needed
        //DEBUG("recalculate readset");
        FD_ZERO(&readset);
        FD_SET(pipe_read_handle, &readset);
        maxfd = pipe_read_handle;
        for (po_peer& pd : peers)
        {
            auto fd = pd.get_socket();
            if (fd > maxfd) maxfd = fd;
            FD_SET(fd, &readset);
        }
        // iterate over key value pairs
        for (auto& kvp : doormen)
        {
            // iterate over value (= list of doormen)
            for (auto& dm : kvp.second)
            {
                auto fd = dm.get_socket();
                if (fd > maxfd) maxfd = fd;
                FD_SET(fd, &readset);
            }
        }
    }
}

} } } // namespace cppa::detail::<anonmyous>

namespace cppa { namespace detail {

void post_office_add_peer(native_socket_t a0,
                          const process_information_ptr& a1,
                          const actor_proxy_ptr& a2,
                          std::unique_ptr<attachable>&& a3)
{
    s_po_manager.m_queue.push_back(new post_office_msg(a0, a1, a2,
                                                        std::move(a3)));
    pipe_msg msg = { rd_queue_event, 0 };
    write(s_po_manager.write_handle(), msg, pipe_msg_size);
}

void post_office_publish(native_socket_t server_socket,
                         const actor_ptr& published_actor)
{
    s_po_manager.m_queue.push_back(new post_office_msg(server_socket,
                                                        published_actor));
    pipe_msg msg = { rd_queue_event, 0 };
    write(s_po_manager.write_handle(), msg, pipe_msg_size);
}

void post_office_unpublish(std::uint32_t actor_id)
{
    pipe_msg msg = { unpublish_actor_event, actor_id };
    write(s_po_manager.write_handle(), msg, pipe_msg_size);
}

void post_office_close_socket(native_socket_t sfd)
{
    pipe_msg msg = { close_socket_event, static_cast<std::uint32_t>(sfd) };
    write(s_po_manager.write_handle(), msg, pipe_msg_size);
}

} } // namespace cppa::detail
