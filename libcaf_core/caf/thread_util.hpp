
#ifndef CAF_THREAD_UTILS_HPP
#define CAF_THREAD_UTILS_HPP

#include <utility>

// decay copy (from clang type_traits)

template <class T>
inline typename std::decay<T>::type decay_copy(T&& t) {
    return std::forward<T>(t);
}

#endif // CAF_THREAD_UTILS_HPP
