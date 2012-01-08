#ifndef DEFAULT_DEALLOCATOR_HPP
#define DEFAULT_DEALLOCATOR_HPP

namespace cppa { namespace util {

template<typename T>
struct default_deallocator
{
    inline void operator()(T* ptr) { delete ptr; }
};

} } // namespace cppa::detail

#endif // DEFAULT_DEALLOCATOR_HPP
