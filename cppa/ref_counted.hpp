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
class ref_counted
{

 public:

    /**
     * @brief Increases reference count by one.
     */
    void ref();

    /**
     * @brief Decreases reference cound by one.
     * @returns @p true if there are still references to this object
     *          (reference count > 0); otherwise @p false.
     */
    bool deref();

    /**
     * @brief Queries if there is exactly one reference.
     * @returns @p true if reference count is one; otherwise @p false.
     */
    bool unique();

};

#else

typedef detail::ref_counted_impl< std::atomic<size_t> > ref_counted;

#endif

} // namespace cppa

#endif // LIBCPPA_REF_COUNTED_HPP
