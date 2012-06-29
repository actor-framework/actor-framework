
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_CONTEXT_SOURCE

#include <boost/context/stack_utils.hpp>

extern "C" {
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
}

#include <cmath>
#include <csignal>

#include <boost/assert.hpp>

namespace {

static rlimit stacksize_limit_()
{
    rlimit limit;
    const int result = ::getrlimit( RLIMIT_STACK, & limit);
    BOOST_ASSERT( 0 == result);
    return limit;
}

static rlimit stacksize_limit()
{
    static rlimit limit = stacksize_limit_();
    return limit;
}

}

namespace boost {
namespace ctx {

BOOST_CONTEXT_DECL
std::size_t default_stacksize()
{
    static std::size_t size = 256 * 1024;
    return size;
}

BOOST_CONTEXT_DECL
std::size_t minimum_stacksize()
{ return SIGSTKSZ; }

BOOST_CONTEXT_DECL
std::size_t maximum_stacksize()
{
    BOOST_ASSERT( ! is_stack_unbound() );
    return static_cast< std::size_t >( stacksize_limit().rlim_max);
}

BOOST_CONTEXT_DECL
bool is_stack_unbound()
{ return RLIM_INFINITY == stacksize_limit().rlim_max; }

BOOST_CONTEXT_DECL
std::size_t pagesize()
{
    static std::size_t pagesize( ::getpagesize() );
    return pagesize;
}

BOOST_CONTEXT_DECL
std::size_t page_count( std::size_t stacksize)
{
    return static_cast< std::size_t >( 
        std::ceil(
            static_cast< float >( stacksize) / pagesize() ) );
}

}}
