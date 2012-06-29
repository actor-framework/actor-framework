
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_CONTEXT_SOURCE

#include <boost/context/stack_allocator.hpp>

extern "C" {
#include <windows.h>
}

#include <stdexcept>

#include <boost/config.hpp>
#include <boost/assert.hpp>
#include <boost/format.hpp>

#include <boost/context/stack_utils.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

# if defined(BOOST_MSVC)
# pragma warning(push)
# pragma warning(disable:4244 4267)
# endif

namespace boost {
namespace ctx {

void *
stack_allocator::allocate( std::size_t size) const
{
    if ( minimum_stacksize() > size)
        throw std::invalid_argument(
            boost::str( boost::format("invalid stack size: must be at least %d bytes")
                % minimum_stacksize() ) );

    if ( ! is_stack_unbound() && ( maximum_stacksize() < size) )
        throw std::invalid_argument(
            boost::str( boost::format("invalid stack size: must not be larger than %d bytes")
                % maximum_stacksize() ) );

    const std::size_t pages( page_count( size) + 1); // add +1 for guard page
    std::size_t size_ = pages * pagesize();

#ifndef BOOST_CONTEXT_FIBER
    void * limit = ::VirtualAlloc( 0, size_, MEM_COMMIT, PAGE_READWRITE);
    if ( ! limit) throw std::bad_alloc();

    DWORD old_options;
    const BOOL result = ::VirtualProtect(
        limit, pagesize(), PAGE_READWRITE | PAGE_GUARD /*PAGE_NOACCESS*/, & old_options);
    BOOST_ASSERT( FALSE != result);

    return static_cast< char * >( limit) + size_;
#endif
}

void
stack_allocator::deallocate( void * vp, std::size_t size) const
{
    if ( vp)
    {
        const std::size_t pages( page_count( size) + 1); // add +1 for guard page
        std::size_t size_ = pages * pagesize();
        BOOST_ASSERT( 0 < size && 0 < size_);
        void * limit = static_cast< char * >( vp) - size_;
        ::VirtualFree( limit, 0, MEM_RELEASE);
    }
}

}}

# if defined(BOOST_MSVC)
# pragma warning(pop)
# endif

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
