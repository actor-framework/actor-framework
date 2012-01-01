#ifndef POST_OFFICE_HPP
#define POST_OFFICE_HPP

#include <memory>

#include "cppa/actor_proxy.hpp"
#include "cppa/detail/native_socket.hpp"

namespace cppa { namespace detail {

void post_office_loop(int pipe_read_handle, int pipe_write_handle);

void post_office_add_peer(native_socket_type peer_socket,
                          process_information_ptr const& peer_ptr,
                          actor_proxy_ptr const& peer_actor_ptr,
                          std::unique_ptr<attachable>&& peer_observer);

void post_office_publish(native_socket_type server_socket,
                         actor_ptr const& published_actor);

void post_office_unpublish(actor_id whom);

void post_office_close_socket(native_socket_type sfd);

} } // namespace cppa::detail

#endif // POST_OFFICE_HPP
