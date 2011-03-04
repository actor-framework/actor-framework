#ifndef CPPA_CONFIG_HPP
#define CPPA_CONFIG_HPP

#if defined(__GNUC__)
#  define CPPA_GCC
#endif

#if defined(__APPLE__)
#  define CPPA_MACOS
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

#endif // CPPA_CONFIG_HPP
