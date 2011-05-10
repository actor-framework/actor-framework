#ifndef DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP
#define DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP

#include "cppa/any_type.hpp"
#include "cppa/util/void_type.hpp"

#include "cppa/util/disjunction.hpp"
#include "cppa/util/is_iterable.hpp"
#include "cppa/util/is_primitive.hpp"
#include "cppa/util/is_forward_iterator.hpp"
#include "cppa/util/uniform_type_info_base.hpp"

#include "cppa/detail/map_member.hpp"
#include "cppa/detail/list_member.hpp"
#include "cppa/detail/primitive_member.hpp"

namespace cppa { namespace detail {

template<typename T>
class is_stl_compliant_list
{

    template<class C>
    static bool cmp_help_fun
    (
        // mutable pointer
        C* mc,
        // check if there's a 'void push_back()' that takes C::value_type
        decltype(mc->push_back(typename C::value_type()))*                  = 0
    )
    {
        return true;
    }

    // SFNINAE default
    static void cmp_help_fun(void*) { }

    typedef decltype(cmp_help_fun(static_cast<T*>(nullptr))) result_type;

 public:

    static const bool value =    util::is_iterable<T>::value
                              && std::is_same<bool, result_type>::value;

};

template<typename T>
class is_stl_compliant_map
{

    template<class C>
    static bool cmp_help_fun
    (
        // mutable pointer
        C* mc,
        // check if there's an 'insert()' that takes C::value_type
        decltype(mc->insert(typename C::value_type()))* = nullptr
    )
    {
        return true;
    }

    static void cmp_help_fun(...) { }

    typedef decltype(cmp_help_fun(static_cast<T*>(nullptr))) result_type;

 public:

    static const bool value =    util::is_iterable<T>::value
                              && std::is_same<bool, result_type>::value;

};

template<typename T>
struct has_default_uniform_type_info_impl
{
    static const bool value =    util::is_primitive<T>::value
                              || is_stl_compliant_map<T>::value
                              || is_stl_compliant_list<T>::value;
};

template<typename Struct>
class default_uniform_type_info_impl : public util::uniform_type_info_base<Struct>
{

    template<typename T>
    struct is_invalid
    {
        static const bool value =    !util::is_primitive<T>::value
                                  && !is_stl_compliant_map<T>::value
                                  && !is_stl_compliant_list<T>::value;
    };

    class member
    {

        uniform_type_info* m_meta;

        std::function<void (const uniform_type_info*,
                            const void*,
                            serializer*      )> m_serialize;

        std::function<void (const uniform_type_info*,
                            void*,
                            deserializer*    )> m_deserialize;

        member(const member&) = delete;
        member& operator=(const member&) = delete;

        void swap(member& other)
        {
            std::swap(m_meta, other.m_meta);
            std::swap(m_serialize, other.m_serialize);
            std::swap(m_deserialize, other.m_deserialize);
        }

        template<typename S, class D>
        member(uniform_type_info* mtptr, S&& s, D&& d)
            : m_meta(mtptr)
            , m_serialize(std::forward<S>(s))
            , m_deserialize(std::forward<D>(d))
        {
        }

     public:

        template<typename T, class C>
        member(uniform_type_info* mtptr, T C::*mem_ptr) : m_meta(mtptr)
        {
            m_serialize = [mem_ptr] (const uniform_type_info* mt,
                                     const void* obj,
                                     serializer* s)
            {
                mt->serialize(&(*reinterpret_cast<const C*>(obj).*mem_ptr), s);
            };
            m_deserialize = [mem_ptr] (const uniform_type_info* mt,
                                       void* obj,
                                       deserializer* d)
            {
                mt->deserialize(&(*reinterpret_cast<C*>(obj).*mem_ptr), d);
            };
        }

        member(member&& other) : m_meta(nullptr)
        {
            swap(other);
        }

        // a member that's not a member at all, but "forwards"
        // the 'self' pointer to make use *_member implementations
        static member fake_member(uniform_type_info* mtptr)
        {
            return {
                mtptr,
                [] (const uniform_type_info* mt, const void* obj, serializer* s)
                {
                    mt->serialize(obj, s);
                },
                [] (const uniform_type_info* mt, void* obj, deserializer* d)
                {
                    mt->deserialize(obj, d);
                }
            };
        }

        ~member()
        {
            delete m_meta;
        }

        member& operator=(member&& other)
        {
            swap(other);
            return *this;
        }

        inline void serialize(const void* parent, serializer* s) const
        {
            m_serialize(m_meta, parent, s);
        }

        inline void deserialize(void* parent, deserializer* d) const
        {
            m_deserialize(m_meta, parent, d);
        }

    };

    std::vector<member> m_members;

    // terminates recursion
    inline void push_back() { }

    // pr.first = member pointer
    // pr.second = meta object to handle pr.first
    template<typename T, class C, typename... Args>
    void push_back(std::pair<T C::*, util::uniform_type_info_base<T>*> pr,
                   const Args&... args)
    {
        m_members.push_back({ pr.second, pr.first });
        push_back(args...);
    }

    template<class C, typename T, typename... Args>
    typename util::enable_if<util::is_primitive<T> >::type
    push_back(T C::*mem_ptr, const Args&... args)
    {
        m_members.push_back({ new primitive_member<T>(), mem_ptr });
        push_back(args...);
    }

    template<class C, typename T, typename... Args>
    typename util::enable_if<is_stl_compliant_list<T> >::type
    push_back(T C::*mem_ptr, const Args&... args)
    {
        m_members.push_back({ new list_member<T>(), mem_ptr });
        push_back(args...);
    }

    template<class C, typename T, typename... Args>
    typename util::enable_if<is_stl_compliant_map<T> >::type
    push_back(T C::*mem_ptr, const Args&... args)
    {
        m_members.push_back({ new map_member<T>(), mem_ptr });
        push_back(args...);
    }

    template<class C, typename T, typename... Args>
    typename util::enable_if<is_invalid<T>>::type
    push_back(T C::*mem_ptr, const Args&... args)
    {
        static_assert(util::is_primitive<T>::value,
                      "T is neither a primitive type nor a "
                      "stl-compliant list/map");
    }

    template<typename T>
    void init_(typename
               util::enable_if<
                   util::disjunction<std::is_same<T,util::void_type>,
                                     std::is_same<T,any_type>>>::type* = 0)
    {
        // any_type doesn't have any fields (no serialization required)
    }

    template<typename T>
    void init_(typename util::enable_if<util::is_primitive<T>>::type* = 0)
    {
        m_members.push_back(member::fake_member(new primitive_member<T>()));
    }

    template<typename T>
    void init_(typename
               util::disable_if<
                   util::disjunction<util::is_primitive<T>,
                                     std::is_same<T,util::void_type>,
                                     std::is_same<T,any_type>>>::type* = 0)
    {
        static_assert(util::is_primitive<T>::value,
                      "T is neither a primitive type nor a "
                      "stl-compliant list/map");
    }

 public:

    template<typename... Args>
    default_uniform_type_info_impl(const Args&... args)
    {
        push_back(args...);
    }

    default_uniform_type_info_impl()
    {
        init_<Struct>();
    }

    void serialize(const void* obj, serializer* s) const
    {
        s->begin_object(this->name());
        for (auto& m : m_members)
        {
            m.serialize(obj, s);
        }
        s->end_object();
    }

    void deserialize(void* obj, deserializer* d) const
    {
        std::string cname = d->seek_object();
        if (cname != this->name())
        {
            throw std::logic_error("wrong type name found");
        }
        d->begin_object(this->name());
        for (auto& m : m_members)
        {
            m.deserialize(obj, d);
        }
        d->end_object();
    }


};

} } // namespace detail

#endif // DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP
