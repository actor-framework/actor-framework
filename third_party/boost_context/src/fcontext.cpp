
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_CONTEXT_SOURCE

#include <boost/context/fcontext.hpp>

#include <cstddef>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace ctx {
namespace detail {

extern "C" BOOST_CONTEXT_DECL
void * BOOST_CONTEXT_CALLDECL align_stack( void * vp)
{
	void * base = vp;
    if ( 0 != ( ( ( uintptr_t) base) & 15) )
        base = ( char * ) ( ( ( ( uintptr_t) base) - 15) & ~0x0F);
	return base;
}

}

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
