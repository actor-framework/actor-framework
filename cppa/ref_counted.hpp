#ifndef LIBCPPA_REF_COUNTED_HPP
#define LIBCPPA_REF_COUNTED_HPP

#include <atomic>
#include <cstddef>

#include "cppa/detail/ref_counted_impl.hpp"

namespace cppa {

/**
 * @brief (Thread safe) base class for reference counted objects
 *        with an atomic reference count.
 */
typedef detail::ref_counted_impl< std::atomic<size_t> > ref_counted;

} // namespace cppa

#endif // LIBCPPA_REF_COUNTED_HPP
