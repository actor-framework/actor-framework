// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

// This convenience header pulls in platform-specific headers for the C socket
// API. Do *not* include this header in other headers.

#pragma once

#include "caf/config.hpp"

// clang-format off
#ifdef CAF_WINDOWS
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif // CAF_WINDOWS
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif // NOMINMAX
#  ifdef CAF_MINGW
#    undef _WIN32_WINNT
#    undef WINVER
#    define _WIN32_WINNT WindowsVista
#    define WINVER WindowsVista
#    include <w32api.h>
#  endif // CAF_MINGW
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2ipdef.h>
#  include <ws2tcpip.h>
#else // CAF_WINDOWS
#  include <sys/types.h>
#  include <arpa/inet.h>
#  include <cerrno>
#  include <fcntl.h>
#  include <netinet/in.h>
#  include <netinet/ip.h>
#  include <netinet/tcp.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  include <sys/uio.h>
#endif
// clang-format on
