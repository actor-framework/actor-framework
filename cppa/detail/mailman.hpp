#ifndef MAILMAN_HPP
#define MAILMAN_HPP

#include "cppa/any_tuple.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/process_information.hpp"
#include "cppa/detail/native_socket.hpp"
#include "cppa/util/single_reader_queue.hpp"

namespace cppa { namespace detail {

struct mailman_send_job
{
    process_information_ptr target_peer;
    any_tuple original_message;
    mailman_send_job(actor_proxy_ptr apptr, any_tuple msg);
    mailman_send_job(process_information_ptr peer, any_tuple msg);
};

struct mailman_add_peer
{
    native_socket_t sockfd;
    process_information_ptr pinfo;
    mailman_add_peer(native_socket_t sfd, const process_information_ptr& pinf);
};

class mailman_job
{

    friend class util::single_reader_queue<mailman_job>;

 public:

    enum job_type
    {
        send_job_type,
        add_peer_type,
        kill_type
    };

    mailman_job(process_information_ptr piptr, const any_tuple& omsg);

    mailman_job(actor_proxy_ptr apptr, const any_tuple& omsg);

    mailman_job(native_socket_t sockfd, const process_information_ptr& pinfo);

    static mailman_job* kill_job();

    ~mailman_job();

    inline mailman_send_job& send_job()
    {
        return m_send_job;
    }

    inline mailman_add_peer& add_peer_job()
    {
        return m_add_socket;
    }

    inline job_type type() const
    {
        return m_type;
    }

    inline bool is_send_job() const
    {
        return m_type == send_job_type;
    }

    inline bool is_add_peer_job() const
    {
        return m_type == add_peer_type;
    }

    inline bool is_kill_job() const
    {
        return m_type == kill_type;
    }

 private:

    mailman_job* next;
    job_type m_type;
    // unrestricted union
    union
    {
        mailman_send_job m_send_job;
        mailman_add_peer m_add_socket;
    };

    mailman_job(job_type jt);

};

void mailman_loop();

util::single_reader_queue<mailman_job>& mailman_queue();

}} // namespace cppa::detail

#endif // MAILMAN_HPP
