#ifndef POST_OFFICE_HPP
#define POST_OFFICE_HPP

#include <memory>

#include "cppa/actor_proxy.hpp"
#include "cppa/detail/native_socket.hpp"

namespace cppa { namespace detail {

void post_office_loop(int pipe_read_handle, int pipe_write_handle);

void post_office_add_peer(native_socket_t peer_socket,
                          const process_information_ptr& peer_ptr,
                          const actor_proxy_ptr& peer_actor_ptr,
                          std::unique_ptr<attachable>&& peer_observer);

void post_office_publish(native_socket_t server_socket,
                         const actor_ptr& published_actor);

void post_office_unpublish(actor_id whom);

void post_office_close_socket(native_socket_t sfd);

} } // namespace cppa::detail

#endif // POST_OFFICE_HPP
