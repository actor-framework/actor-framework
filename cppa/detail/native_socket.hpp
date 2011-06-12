#ifndef NATIVE_SOCKET_HPP
#define NATIVE_SOCKET_HPP

#include "cppa/config.hpp"

#ifdef CPPA_WINDOWS
#else
#   include <netdb.h>
#   include <unistd.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#endif

namespace cppa { namespace detail {

#ifdef CPPA_WINDOWS
    typedef SOCKET native_socket_t;
    typedef const char* socket_send_ptr;
    typedef char* socket_recv_ptr;
#else
    typedef int native_socket_t;
    typedef const void* socket_send_ptr;
    typedef void* socket_recv_ptr;
    void closesocket(native_socket_t s);
#endif

} } // namespace cppa::detail

#endif // NATIVE_SOCKET_HPP
