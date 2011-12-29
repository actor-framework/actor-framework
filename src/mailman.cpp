#include <atomic>
#include <iostream>

#include "cppa/to_string.hpp"
#include "cppa/detail/mailman.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/detail/post_office.hpp"

//#define DEBUG(arg) std::cout << arg << std::endl
#define DEBUG(unused) ((void) 0)

// implementation of mailman.hpp
namespace cppa { namespace detail {

mailman_job::mailman_job(process_information_ptr piptr,
                         const actor_ptr& from,
                         const channel_ptr& to,
                         const any_tuple& content)
    : next(nullptr), m_type(send_job_type)
{
    new (&m_send_job) mailman_send_job (piptr, from, to, content);
}

mailman_job::mailman_job(native_socket_type sockfd, const process_information_ptr& pinfo)
    : next(0), m_type(add_peer_type)
{
    new (&m_add_socket) mailman_add_peer (sockfd, pinfo);
}

mailman_job* mailman_job::kill_job()
{
    return new mailman_job(kill_type);
}

mailman_job::~mailman_job()
{
    switch (m_type)
    {
        case send_job_type:
        {
            m_send_job.~mailman_send_job();
            break;
        }
        case add_peer_type:
        {
            m_add_socket.~mailman_add_peer();
            break;
        }
        case kill_type:
        {
            // union doesn't contain a valid object
            break;
        }
    }
}

/*
// implemented in post_office.cpp
util::single_reader_queue<mailman_job>& mailman_queue();
*/

// known issues: send() should be asynchronous and select() should be used
void mailman_loop()
{
    // serializes outgoing messages
    binary_serializer bs;
    // current active job
    mailman_job* job = nullptr;
    // caches mailman_queue()
    auto& mqueue = mailman_queue();
    // connected tcp peers
    std::map<process_information, native_socket_type> peers;
    for (;;)
    {
        job = mqueue.pop();
        if (job->is_send_job())
        {
            mailman_send_job& sjob = job->send_job();
            // forward message to receiver peer
            auto peer_element = peers.find(*(sjob.target_peer));
            if (peer_element != peers.end())
            {
                bool disconnect_peer = false;
                auto peer = peer_element->second;
                try
                {
                    bs << sjob.msg;
                    auto size32 = static_cast<std::uint32_t>(bs.size());
                    DEBUG("--> " << to_string(sjob.msg));
                    // write size of serialized message
                    auto sent = ::send(peer, &size32, sizeof(std::uint32_t), 0);
                    if (sent > 0)
                    {
                        // write message
                        sent = ::send(peer, bs.data(), bs.size(), 0);
                    }
                    // disconnect peer if send() failed
                    disconnect_peer = (sent <= 0);
                    // make sure all bytes are written
                    if (static_cast<std::uint32_t>(sent) != size32)
                    {
                        throw std::logic_error("send() not a synchronous socket");
                    }
                }
                // something went wrong; close connection to this peer
                catch (std::exception& e)
                {
                    DEBUG(to_uniform_name(typeid(e)) << ": " << e.what());
                    disconnect_peer = true;
                }
                if (disconnect_peer)
                {
                    DEBUG("peer disconnected (error during send)");
                    //closesocket(peer);
                    post_office_close_socket(peer);
                    peers.erase(peer_element);
                }
                bs.reset();
            }
            else
            {
                DEBUG("message to an unknown peer");
            }
            // else: unknown peer
        }
        else if (job->is_add_peer_job())
        {
            mailman_add_peer& pjob = job->add_peer_job();
            auto i = peers.find(*(pjob.pinfo));
            if (i == peers.end())
            {
                //cout << "mailman added " << pjob.pinfo->process_id() << "@"
                //     << to_string(pjob.pinfo->node_id()) << endl;
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

} } // namespace cppa::detail
