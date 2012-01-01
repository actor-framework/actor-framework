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

    inline static T const& deref(void const* ptr)
    {
        return *reinterpret_cast<T const*>(ptr);
    }

    inline static T& deref(void* ptr)
    {
        return *reinterpret_cast<T*>(ptr);
    }

 protected:

    abstract_uniform_type_info(std::string const& uname
                           = detail::to_uniform_name(typeid(T)))
        : uniform_type_info(uname)
    {
    }

    bool equals(void const* lhs, void const* rhs) const
    {
        return deref(lhs) == deref(rhs);
    }

    void* new_instance(void const* ptr) const
    {
        return (ptr) ? new T(deref(ptr)) : new T();
    }

    void delete_instance(void* instance) const
    {
        delete reinterpret_cast<T*>(instance);
    }

 public:

    bool equals(std::type_info const& tinfo) const
    {
        return typeid(T) == tinfo;
    }

};

} }

#endif // ABSTRACT_UNIFORM_TYPE_INFO_HPP
