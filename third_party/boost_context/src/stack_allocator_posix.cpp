
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_CONTEXT_SOURCE

#include <boost/context/stack_allocator.hpp>

extern "C" {
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
}

#include <stdexcept>

#include <boost/config.hpp>
#include <boost/assert.hpp>
#include <boost/format.hpp>

#include <boost/context/stack_utils.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

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

    const int fd( ::open("/dev/zero", O_RDONLY) );
    BOOST_ASSERT( -1 != fd);
    void * limit =
# if defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)
        ::mmap( 0, size_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
# else
        ::mmap( 0, size_, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
# endif
    ::close( fd);
    if ( ! limit) throw std::bad_alloc();

    const int result( ::mprotect( limit, pagesize(), PROT_NONE) );
    BOOST_ASSERT( 0 == result);

    return static_cast< char * >( limit) + size_;
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
        ::munmap( limit, size_);
    }
}

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
