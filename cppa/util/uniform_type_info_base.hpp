#ifndef UNIFORM_TYPE_INFO_BASE_HPP
#define UNIFORM_TYPE_INFO_BASE_HPP

#include "cppa/uniform_type_info.hpp"
#include "cppa/detail/to_uniform_name.hpp"

namespace cppa { namespace util {

/**
 * @brief Implements all pure virtual functions of {@link uniform_type_info}
 *        except serialize() and deserialize().
 */
template<typename T>
class uniform_type_info_base : public uniform_type_info
{

    inline static const T& deref(const void* ptr)
    {
        return *reinterpret_cast<const T*>(ptr);
    }

    inline static T& deref(void* ptr)
    {
        return *reinterpret_cast<T*>(ptr);
    }

 protected:

    uniform_type_info_base(const std::string& uname
                           = detail::to_uniform_name(typeid(T)))
        : uniform_type_info(uname)
    {
    }

    bool equal(const void* lhs, const void* rhs) const
    {
        return deref(lhs) == deref(rhs);
    }

    void* new_instance(const void* ptr) const
    {
        return (ptr) ? new T(deref(ptr)) : new T();
    }

    void delete_instance(void* instance) const
    {
        delete reinterpret_cast<T*>(instance);
    }

 public:

    bool equal(const std::type_info& tinfo) const
    {
        return typeid(T) == tinfo;
    }

};

} }

#endif // UNIFORM_TYPE_INFO_BASE_HPP
