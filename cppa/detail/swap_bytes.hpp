#ifndef SWAP_BYTES_HPP
#define SWAP_BYTES_HPP

#include <cstddef>

namespace cppa { namespace detail {

template<typename T>
struct byte_access
{
    union
    {
        T value;
        unsigned char bytes[sizeof(T)];
    };
    inline byte_access(T val = 0) : value(val) { }
};

template<size_t SizeOfT, typename T>
struct byte_swapper
{
    static T _(byte_access<T> from)
    {
        byte_access<T> to;
        auto i = SizeOfT - 1;
        for (size_t j = 0 ; j < SizeOfT ; --i, ++j)
        {
            to.bytes[i] = from.bytes[j];
        }
        return to.value;
    }
};

template<typename T>
struct byte_swapper<1, T>
{
    inline static T _(T what) { return what; }
};

template<typename T>
inline T swap_bytes(T what)
{
    return byte_swapper<sizeof(T), T>::_(what);
}

} } // namespace cppa::detail

#endif // SWAP_BYTES_HPP
