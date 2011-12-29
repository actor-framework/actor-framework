#include "cppa/config.hpp"

#include <ios> // ios_base::failure
#include <errno.h>
#include <sstream>
#include "cppa/detail/native_socket.hpp"

namespace cppa { namespace detail {

#ifndef CPPA_WINDOWS

void closesocket(native_socket_type s)
{
    if(::close(s) != 0)
    {
        switch(errno)
        {
            case EBADF:
            {
                throw std::ios_base::failure("EBADF: invalid socket");
            }
            case EINTR:
            {
                throw std::ios_base::failure("EINTR: interrupted");
            }
            case EIO:
            {
                throw std::ios_base::failure("EIO: an I/O error occured");
            }
            default:
            {
                std::ostringstream oss;
                throw std::ios_base::failure("");
            }
        }
    }
}

#endif

} } // namespace cppa::detail
