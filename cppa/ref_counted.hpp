#ifndef LIBCPPA_REF_COUNTED_HPP
#define LIBCPPA_REF_COUNTED_HPP

#include <atomic>
#include <cstddef>

#include "cppa/detail/ref_counted_impl.hpp"

namespace cppa {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief A (thread safe) base class for reference counted objects
 *        with an atomic reference count.
 *
 * Serves the requirements of {@link intrusive_ptr}.
 * @relates intrusive_ptr
 */
class ref_counted { };

#else

typedef detail::ref_counted_impl< std::atomic<size_t> > ref_counted;

#endif

} // namespace cppa

#endif // LIBCPPA_REF_COUNTED_HPP
