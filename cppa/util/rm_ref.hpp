#ifndef RM_REF_HPP
#define RM_REF_HPP

namespace cppa { namespace util {

/**
 * @brief Like std::remove_reference but prohibits void and
 *        also removes const references.
 */
template<typename T>
struct rm_ref { typedef T type; };

template<typename T>
struct rm_ref<T const&> { typedef T type; };

template<typename T>
struct rm_ref<T&> { typedef T type; };

template<>
struct rm_ref<void> { };

} } // namespace cppa::util

#endif // RM_REF_HPP
