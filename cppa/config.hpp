#ifndef CPPA_CONFIG_HPP
#define CPPA_CONFIG_HPP

#if defined(__GNUC__)
#  define CPPA_GCC
#endif

#if defined(__APPLE__)
#  define CPPA_MACOS
#  ifndef _GLIBCXX_HAS_GTHREADS
#    define _GLIBCXX_HAS_GTHREADS
#  endif
#elif defined(__GNUC__) && defined(__linux__)
#  define CPPA_LINUX
#elif defined(WIN32)
#  define CPPA_WINDOWS
#else
#  error Plattform and/or compiler not supportet
#endif

#if defined(__amd64__) || defined(__LP64__)
#  define CPPA_64BIT
#endif

#ifdef CPPA_MACOS
#   include <libkern/OSAtomic.h>
#   define CPPA_MEMORY_BARRIER() OSMemoryBarrier()
#elif defined(CPPA_LINUX)
#   define CPPA_MEMORY_BARRIER() __sync_synchronize()
#endif

#endif // CPPA_CONFIG_HPP
