#ifndef HASH_OF_HPP
#define HASH_OF_HPP

#include <string>
#include <ostream>
#include <cstdint>
#include <stdexcept>

unsigned int hash_of(const std::string& what);
unsigned int hash_of(const char* what, int what_length);

struct hash_result_160bit
{
    std::uint8_t data[20];
    inline std::uint8_t& operator[](size_t p)
    {
        return data[p];
    }
    inline std::uint8_t operator[](size_t p) const
    {
        return data[p];
    }
    inline size_t size() const
    {
        return 20;
    }
};

namespace std {

inline ostream& operator<<(ostream& o, const hash_result_160bit& res)
{
    auto flags = o.flags();
    o << std::hex;
    for (size_t i = 0; i < res.size(); ++i)
    {
        o.width(2);
        o.fill('0');
        o << static_cast<std::uint32_t>(res[i]);
    }
    // restore flags afterwards
    o.flags(flags);
    return o;
}

} // namespace std

hash_result_160bit ripemd_160(const std::string& data);

#endif // HASH_OF_HPP
