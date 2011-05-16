#ifndef ABSTRACT_UNIFORM_TYPE_INFO_HPP
#define ABSTRACT_UNIFORM_TYPE_INFO_HPP

#include "cppa/uniform_type_info.hpp"
#include "cppa/detail/to_uniform_name.hpp"

namespace cppa { namespace util {

/**
 * @brief Implements all pure virtual functions of {@link uniform_type_info}
 *        except serialize() and deserialize().
 */
template<typename T>
class abstract_uniform_type_info : public uniform_type_info
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

    abstract_uniform_type_info(const std::string& uname
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

#endif // ABSTRACT_UNIFORM_TYPE_INFO_HPP
