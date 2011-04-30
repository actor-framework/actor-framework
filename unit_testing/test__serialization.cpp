#include <new>
#include <set>
#include <locale>
#include <memory>
#include <string>
#include <limits>
#include <vector>
#include <cstring>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <iterator>
#include <typeinfo>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <type_traits>

#include "test.hpp"

#include "cppa/util.hpp"
#include "cppa/match.hpp"
#include "cppa/tuple.hpp"
#include "cppa/serializer.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/untyped_tuple.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/disable_if.hpp"
#include "cppa/util/if_else_type.hpp"
#include "cppa/util/wrapped_type.hpp"

//#include "cppa/util.hpp"

using std::cout;
using std::cerr;
using std::endl;

using namespace cppa::util;

template<class C, template <typename> class... Traits>
struct apply;

template<class C>
struct apply<C>
{
    typedef C type;
};

template<class C,
         template <typename> class Trait0,
         template <typename> class... Traits>
struct apply<C, Trait0, Traits...>
{
    typedef typename apply<typename Trait0<C>::type, Traits...>::type type;
};


template<typename T>
struct plain
{
    typedef typename apply<T, std::remove_reference, std::remove_cv>::type type;
};

/**
 * @brief Integers (signed and unsigned), floating points and strings.
 */
enum primitive_type
{
    pt_int8,        pt_int16,       pt_int32,        pt_int64,
    pt_uint8,       pt_uint16,      pt_uint32,       pt_uint64,
    pt_float,       pt_double,      pt_long_double,
    pt_u8string,    pt_u16string,   pt_u32string,
    pt_null
};

constexpr const char* primitive_type_names[] =
{
    "pt_int8",        "pt_int16",       "pt_int32",       "pt_int64",
    "pt_uint8",       "pt_uint16",      "pt_uint32",      "pt_uint64",
    "pt_float",       "pt_double",      "pt_long_double",
    "pt_u8string",    "pt_u16string",   "pt_u32string",
    "pt_null"
};

constexpr const char* primitive_type_name(primitive_type ptype)
{
    return primitive_type_names[static_cast<int>(ptype)];
}

// achieves static call dispatch (Int-To-Type idiom)
template<primitive_type FT>
struct pt_token { static const primitive_type value = FT; };

// maps the fundamental_type FT to the corresponding type
template<primitive_type FT>
struct ptype_to_type
  : if_else_type_c<FT == pt_int8, std::int8_t,
    if_else_type_c<FT == pt_int16, std::int16_t,
    if_else_type_c<FT == pt_int32, std::int32_t,
    if_else_type_c<FT == pt_int64, std::int64_t,
    if_else_type_c<FT == pt_uint8, std::uint8_t,
    if_else_type_c<FT == pt_uint16, std::uint16_t,
    if_else_type_c<FT == pt_uint32, std::uint32_t,
    if_else_type_c<FT == pt_uint64, std::uint64_t,
    if_else_type_c<FT == pt_float, float,
    if_else_type_c<FT == pt_double, double,
    if_else_type_c<FT == pt_long_double, long double,
    if_else_type_c<FT == pt_u8string, std::string,
    if_else_type_c<FT == pt_u16string, std::u16string,
    if_else_type_c<FT == pt_u32string, std::u32string,
    wrapped_type<void> > > > > > > > > > > > > > >
{
};

// if (IfStmt == true) ptype = FT; else ptype = Else::ptype;
template<bool IfStmt, primitive_type FT, class Else>
struct if_else_ptype_c
{
    static const primitive_type ptype = FT;
};

template<primitive_type FT, class Else>
struct if_else_ptype_c<false, FT, Else>
{
    static const primitive_type ptype = Else::ptype;
};

// if (Stmt::value == true) ptype = FT; else ptype = Else::ptype;
template<class Stmt, primitive_type FT, class Else>
struct if_else_ptype : if_else_ptype_c<Stmt::value, FT, Else> { };

template<primitive_type FT>
struct wrapped_ptype { static const primitive_type ptype = FT; };

// maps type T the the corresponding fundamental_type
template<typename T>
struct type_to_ptype_impl
      // signed integers
    : if_else_ptype<std::is_same<T, std::int8_t>, pt_int8,
      if_else_ptype<std::is_same<T, std::int16_t>, pt_int16,
      if_else_ptype<std::is_same<T, std::int32_t>, pt_int32,
      if_else_ptype<std::is_same<T, std::int64_t>, pt_int64,
      if_else_ptype<std::is_same<T, std::uint8_t>, pt_uint8,
      // unsigned integers
      if_else_ptype<std::is_same<T, std::uint16_t>, pt_uint16,
      if_else_ptype<std::is_same<T, std::uint32_t>, pt_uint32,
      if_else_ptype<std::is_same<T, std::uint64_t>, pt_uint64,
      // float / double
      if_else_ptype<std::is_same<T, float>, pt_float,
      if_else_ptype<std::is_same<T, double>, pt_double,
      if_else_ptype<std::is_same<T, long double>, pt_long_double,
      // strings
      if_else_ptype<std::is_convertible<T, std::string>, pt_u8string,
      if_else_ptype<std::is_convertible<T, std::u16string>, pt_u16string,
      if_else_ptype<std::is_convertible<T, std::u32string>, pt_u32string,
      wrapped_ptype<pt_null> > > > > > > > > > > > > > >
{
};

template<typename T>
struct type_to_ptype : type_to_ptype_impl<typename plain<T>::type> { };

template<typename T>
struct is_primitive
{
    static const bool value = type_to_ptype<T>::ptype != pt_null;
};

template<typename T>
class is_iterable
{

    template<class C>
    static bool cmp_help_fun(C& arg0,
                             decltype((arg0.begin() == arg0.end()) &&
                                      (*(++(arg0.begin())) == *(arg0.end())))*)
    {
        return true;
    }

    template<class C>
    static void cmp_help_fun(C&, void*) { }

    typedef decltype(cmp_help_fun(*static_cast<T*>(nullptr),
                                  static_cast<bool*>(nullptr)))
            result_type;

 public:

    static const bool value =    is_primitive<T>::value == false
                              && std::is_same<bool, result_type>::value;

};

template<typename T>
class has_push_back
{

    template<class C>
    static bool cmp_help_fun(C* arg0,
                             decltype(arg0->push_back(typename C::value_type()))*)
    {
        return true;
    }

    static void cmp_help_fun(void*, void*) { }

    typedef decltype(cmp_help_fun(static_cast<T*>(nullptr),
                                  static_cast<void*>(nullptr)))
            result_type;

 public:

    static const bool value =    is_iterable<T>::value
                              && std::is_same<bool, result_type>::value;

};

template<typename T>
class has_insert
{

    template<class C>
    static bool cmp_help_fun(C* arg0,
                             decltype((arg0->insert(typename C::value_type())).second)*)
    {
        return true;
    }

    static void cmp_help_fun(void*, void*) { }

    typedef decltype(cmp_help_fun(static_cast<T*>(nullptr),
                                  static_cast<bool*>(nullptr)))
            result_type;

 public:

    static const bool value =    is_iterable<T>::value
                              && std::is_same<bool, result_type>::value;

};

class pt_value;

template<typename T>
T pt_value_cast(pt_value&);

template<primitive_type FT>
typename ptype_to_type<FT>::type& pt_value_cast(pt_value&);

// Utility function for static call dispatching.
// Invokes pt_token<X>(), where X is the value of ptype.
// Does nothing if ptype == pt_null
template<typename Fun>
void pt_invoke(primitive_type ptype, Fun&& f)
{
    switch (ptype)
    {
     case pt_int8:        f(pt_token<pt_int8>());        break;
     case pt_int16:       f(pt_token<pt_int16>());       break;
     case pt_int32:       f(pt_token<pt_int32>());       break;
     case pt_int64:       f(pt_token<pt_int64>());       break;
     case pt_uint8:       f(pt_token<pt_uint8>());       break;
     case pt_uint16:      f(pt_token<pt_uint16>());      break;
     case pt_uint32:      f(pt_token<pt_uint32>());      break;
     case pt_uint64:      f(pt_token<pt_uint64>());      break;
     case pt_float:       f(pt_token<pt_float>());       break;
     case pt_double:      f(pt_token<pt_double>());      break;
     case pt_long_double: f(pt_token<pt_long_double>()); break;
     case pt_u8string:    f(pt_token<pt_u8string>());    break;
     case pt_u16string:   f(pt_token<pt_u16string>());   break;
     case pt_u32string:   f(pt_token<pt_u32string>());   break;
     default: break;
    }
}

/**
 * @brief Describes a value of a {@link primitive_type primitive data type}.
 */
class pt_value
{

    template<typename T>
    friend T pt_value_cast(pt_value&);

    template<primitive_type PT>
    friend typename ptype_to_type<PT>::type& pt_value_cast(pt_value&);

    primitive_type m_ptype;

    union
    {
        std::int8_t i8;
        std::int16_t i16;
        std::int32_t i32;
        std::int64_t i64;
        std::uint8_t u8;
        std::uint16_t u16;
        std::uint32_t u32;
        std::uint64_t u64;
        float fl;
        double dbl;
        long double ldbl;
        std::string str8;
        std::u16string str16;
        std::u32string str32;
    };

    // use static call dispatching to select member variable
    inline decltype(i8)&    get(pt_token<pt_int8>)        { return i8;    }
    inline decltype(i16)&   get(pt_token<pt_int16>)       { return i16;   }
    inline decltype(i32)&   get(pt_token<pt_int32>)       { return i32;   }
    inline decltype(i64)&   get(pt_token<pt_int64>)       { return i64;   }
    inline decltype(u8)&    get(pt_token<pt_uint8>)       { return u8;    }
    inline decltype(u16)&   get(pt_token<pt_uint16>)      { return u16;   }
    inline decltype(u32)&   get(pt_token<pt_uint32>)      { return u32;   }
    inline decltype(u64)&   get(pt_token<pt_uint64>)      { return u64;   }
    inline decltype(fl)&    get(pt_token<pt_float>)       { return fl;    }
    inline decltype(dbl)&   get(pt_token<pt_double>)      { return dbl;   }
    inline decltype(ldbl)&  get(pt_token<pt_long_double>) { return ldbl;  }
    inline decltype(str8)&  get(pt_token<pt_u8string>)    { return str8;  }
    inline decltype(str16)& get(pt_token<pt_u16string>)   { return str16; }
    inline decltype(str32)& get(pt_token<pt_u32string>)   { return str32; }

    template<primitive_type FT, class T, class V>
    static void set(primitive_type& lhs_type, T& lhs, V&& rhs,
                    typename disable_if< std::is_arithmetic<T> >::type* = 0)
    {
        if (FT == lhs_type)
        {
            lhs = std::forward<V>(rhs);
        }
        else
        {
            new (&lhs) T(std::forward<V>(rhs));
            lhs_type = FT;
        }
    }

    template<primitive_type FT, class T, class V>
    static void set(primitive_type& lhs_type, T& lhs, V&& rhs,
                    typename enable_if<std::is_arithmetic<T>, int>::type* = 0)
    {
        // don't call a constructor for arithmetic types
        lhs = rhs;
        lhs_type = FT;
    }

    template<class T>
    inline static void destroy(T&,
                    typename enable_if<std::is_arithmetic<T>, int>::type* = 0)
    {
        // arithmetic types don't need destruction
    }

    template<class T>
    inline static void destroy(T& what,
                    typename disable_if< std::is_arithmetic<T> >::type* = 0)
    {
        what.~T();
    }

    struct destroyer
    {
        pt_value* m_self;
        inline destroyer(pt_value* self) : m_self(self) { }
        template<primitive_type FT>
        inline void operator()(pt_token<FT> token) const
        {
            destroy(m_self->get(token));
        }
    };

    struct initializer
    {
        pt_value* m_self;
        inline initializer(pt_value* self) : m_self(self) { }
        template<primitive_type FT>
        inline void operator()(pt_token<FT> token) const
        {
            set<FT>(m_self->m_ptype,
                    m_self->get(token),
                    typename ptype_to_type<FT>::type());
        }
    };

    struct setter
    {
        pt_value* m_self;
        const pt_value& m_other;
        inline setter(pt_value* self, const pt_value& other)
            : m_self(self), m_other(other) { }
        template<primitive_type FT>
        inline void operator()(pt_token<FT> token) const
        {
            set<FT>(m_self->m_ptype,
                    m_self->get(token),
                    m_other.get(token));
        }
    };

    struct mover
    {
        pt_value* m_self;
        const pt_value& m_other;
        inline mover(pt_value* self, const pt_value& other)
            : m_self(self), m_other(other) { }
        template<primitive_type FT>
        inline void operator()(pt_token<FT> token) const
        {
            set<FT>(m_self->m_ptype,
                    m_self->get(token),
                    std::move(m_other.get(token)));
        }
    };

    struct comparator
    {
        bool m_result;
        const pt_value* m_self;
        const pt_value& m_other;
        inline comparator(const pt_value* self, const pt_value& other)
            : m_result(false), m_self(self), m_other(other) { }
        template<primitive_type FT>
        inline void operator()(pt_token<FT> token)
        {
            if (m_other.m_ptype == FT)
            {
                m_result = (m_self->get(token) == m_other.get(token));
            }
        }
        inline bool result() const { return m_result; }
    };

    template<class Self, typename Fun>
    struct applier
    {
        Self* m_self;
        Fun& m_f;
        applier(Self* self, Fun& f) : m_self(self), m_f(f) { }
        template<primitive_type FT>
        inline void operator()(pt_token<FT> token)
        {
            m_f(m_self->get(token));
        }
    };

    struct type_reader
    {
        const std::type_info* tinfo;
        type_reader() : tinfo(nullptr) { }
        template<primitive_type FT>
        inline void operator()(pt_token<FT>)
        {
            tinfo = &typeid(typename ptype_to_type<FT>::type);
        }
    };

    void destroy()
    {
        pt_invoke(m_ptype, destroyer(this));
        m_ptype = pt_null;
    }

 public:

    // get(...) const overload
    template<primitive_type FT>
    const typename ptype_to_type<FT>::type& get(pt_token<FT> token) const
    {
        return const_cast<pt_value*>(this)->get(token);
    }

    template<typename Fun>
    void apply(Fun&& f)
    {
        pt_invoke(m_ptype, applier<pt_value, Fun>(this, f));
    }

    template<typename Fun>
    void apply(Fun&& f) const
    {
        pt_invoke(m_ptype, applier<const pt_value, Fun>(this, f));
    }

    pt_value() : m_ptype(pt_null) { }

    template<typename V>
    pt_value(V&& value) : m_ptype(pt_null)
    {
        static_assert(type_to_ptype<V>::ptype != pt_null,
                      "V is not a primitive type");
        set<type_to_ptype<V>::ptype>(m_ptype,
                                     get(pt_token<type_to_ptype<V>::ptype>()),
                                     std::forward<V>(value));
    }

    pt_value(primitive_type ptype) : m_ptype(pt_null)
    {
        pt_invoke(ptype, initializer(this));
    }

    pt_value(const pt_value& other) : m_ptype(pt_null)
    {
        //invoke(setter(other));
        pt_invoke(other.m_ptype, setter(this, other));
    }

    pt_value(pt_value&& other) : m_ptype(pt_null)
    {
        //invoke(mover(other));
        pt_invoke(other.m_ptype, mover(this, other));
    }

    pt_value& operator=(const pt_value& other)
    {
        //invoke(setter(other));
        pt_invoke(other.m_ptype, setter(this, other));
        return *this;
    }

    pt_value& operator=(pt_value&& other)
    {
        //invoke(mover(other));
        pt_invoke(other.m_ptype, mover(this, other));
        return *this;
    }

    bool operator==(const pt_value& other) const
    {
        comparator cmp(this, other);
        pt_invoke(m_ptype, cmp);
        return cmp.result();
    }

    inline bool operator!=(const pt_value& other) const
    {
        return !(*this == other);
    }

    inline primitive_type ptype() const { return m_ptype; }

    const std::type_info& type() const
    {
        type_reader tr;
        pt_invoke(m_ptype, tr);
        return (tr.tinfo) ? *tr.tinfo : typeid(void);
    }

    ~pt_value() { destroy(); }

};

template<typename T>
typename enable_if<is_primitive<T>, bool>::type
operator==(const T& lhs, const pt_value& rhs)
{
    static constexpr primitive_type ptype = type_to_ptype<T>::ptype;
    static_assert(ptype != pt_null, "T couldn't be mapped to an ptype");
    return (rhs.ptype() == ptype) ? lhs == pt_value_cast<ptype>(rhs) : false;
}

template<typename T>
typename enable_if<is_primitive<T>, bool>::type
operator==(const pt_value& lhs, const T& rhs)
{
    return (rhs == lhs);
}

template<typename T>
typename enable_if<is_primitive<T>, bool>::type
operator!=(const pt_value& lhs, const T& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
typename enable_if<is_primitive<T>, bool>::type
operator!=(const T& lhs, const pt_value& rhs)
{
    return !(lhs == rhs);
}

template<primitive_type FT>
typename ptype_to_type<FT>::type& pt_value_cast(pt_value& v)
{
    if (v.ptype() != FT) throw std::bad_cast();
    return v.get(pt_token<FT>());
}

template<primitive_type FT>
const typename ptype_to_type<FT>::type& pt_value_cast(const pt_value& v)
{
    if (v.ptype() != FT) throw std::bad_cast();
    return v.get(pt_token<FT>());
}

template<typename T>
T pt_value_cast(pt_value& v)
{
    static const primitive_type ptype = type_to_ptype<T>::ptype;
    if (v.ptype() != ptype) throw std::bad_cast();
    return v.get(pt_token<ptype>());
}

template<typename T>
T pt_value_cast(const pt_value& v)
{
    typedef typename std::remove_reference<T>::type plain_t;
    static_assert(!std::is_reference<T>::value || std::is_const<plain_t>::value,
                  "Could not get a non-const reference from const pt_value&");
    static const primitive_type ptype = type_to_ptype<T>::ptype;
    if (v.ptype() != ptype) throw std::bad_cast();
    return v.get(pt_token<ptype>());
}

struct getter_setter_pair
{

    std::function<pt_value (void*)> getter;
    std::function<void (void*, pt_value&&)> setter;

    getter_setter_pair(getter_setter_pair&&) = default;
    getter_setter_pair(const getter_setter_pair&) = default;

    template<class C, typename T>
    getter_setter_pair(T C::*member_ptr)
    {
        getter = [=] (void* self) -> pt_value {
            return *reinterpret_cast<C*>(self).*member_ptr;
        };
        setter = [=] (void* self, pt_value&& value) {
            *reinterpret_cast<C*>(self).*member_ptr = std::move(pt_value_cast<T&>(value));
        };
    }

    template<class C, typename GT, typename ST>
    getter_setter_pair(GT (C::*get_memfn)() const, void (C::*set_memfn)(ST))
    {
        getter = [=] (void* self) -> pt_value {
            return (*reinterpret_cast<C*>(self).*get_memfn)();
        };
        setter = [=] (void* self, pt_value&& value) {
            (*reinterpret_cast<C*>(self).*set_memfn)(std::move(pt_value_cast<typename plain<ST>::type&>(value)));
        };
    }

};

class serializer
{

 public:

    virtual void begin_object(const std::string& type_name) = 0;
    virtual void end_object() = 0;

    virtual void begin_sequence(size_t size) = 0;
    virtual void end_sequence() = 0;

    /**
     * @brief Writes a single value.
     */
    virtual void write_value(const pt_value& value) = 0;

    /**
     * @brief Writes @p size values.
     */
    virtual void write_tuple(size_t size, const pt_value* values) = 0;

};

class deserializer
{

 public:

    /**
     * @brief Seeks the beginning of the next object and return its type name.
     */
    virtual std::string seek_object() = 0;

    /**
     * @brief Equal to {@link seek_object()} but doesn't
     *        modify the internal in-stream position.
     */
    virtual std::string peek_object() = 0;

    virtual void begin_object(const std::string& type_name) = 0;
    virtual void end_object() = 0;

    virtual size_t begin_sequence() = 0;
    virtual void end_sequence() = 0;

    virtual pt_value read_value(primitive_type ptype) = 0;

    virtual void read_tuple(size_t size,
                            const primitive_type* ptypes,
                            pt_value* storage             ) = 0;

};

class meta_type
{

 public:

    virtual ~meta_type() { }

    /**
     * @brief Creates an instance of this type, initialized
     *        with the default constructor.
     */
    virtual void* new_instance() const = 0;

    /**
     * @brief Cast @p instance to the native type and delete it.
     */
    virtual void delete_instance(void* instance) const = 0;

    /**
     * @brief Serializes @p instance to @p sink.
     */
    virtual void serialize(const void* instance, serializer* sink) const = 0;

    /**
     * @brief Deserializes @p instance from @p source.
     */
    virtual void deserialize(void* instance, deserializer* source) const = 0;

};

std::map<std::string, meta_type*> s_meta_types;

class root_object
{

 public:

    /**
     * @brief Deserializes a new object from @p source and returns the
     *        new (deserialized) instance with its meta_type.
     */
    std::pair<void*, meta_type*> deserialize(deserializer* source) const
    {
        void* result;
        std::string tname = source->peek_object();
        auto i = s_meta_types.find(tname);
        if (i == s_meta_types.end())
        {
            throw std::logic_error("no meta object found for " + tname);
        }
        auto mobj = i->second;
        if (mobj == nullptr)
        {
            throw std::logic_error("mobj == nullptr");
        }
        result = mobj->new_instance();
        if (result == nullptr)
        {
            throw std::logic_error("result == nullptr");
        }
        try
        {
            mobj->deserialize(result, source);
        }
        catch (...)
        {
            mobj->delete_instance(result);
            return { nullptr, nullptr };
        }
        return { result, mobj };
    }

};

/**
 * @brief {@link meta_type} implementation for primitive data types.
 */
template<typename T>
class primitive_member : public meta_type
{

    static constexpr primitive_type ptype = type_to_ptype<T>::ptype;

    static_assert(ptype != pt_null, "T is not a primitive type");

 public:

    void* new_instance() const
    {
        return new T();
    }

    void delete_instance(void* ptr) const
    {
        delete reinterpret_cast<T*>(ptr);
    }

    void serialize(const void* obj, serializer* s) const
    {
        s->write_value(*reinterpret_cast<const T*>(obj));
    }

    void deserialize(void* obj, deserializer* d) const
    {
        pt_value val(d->read_value(ptype));
        *reinterpret_cast<T*>(obj) = std::move(pt_value_cast<T&>(val));
    }

};

/**
 * @brief {@link meta_type} implementation for STL compliant
 *        lists (such as std::vector and std::list).
 *
 * This implementation requires a primitive List::value_type.
 */
template<typename List>
class list_member : public meta_type
{

    typedef typename List::value_type value_type;
    static constexpr primitive_type vptype = type_to_ptype<value_type>::ptype;

    static_assert(vptype != pt_null,
                  "List::value_type is not a primitive data type");

 public:

    void serialize(const void* obj, serializer* s) const
    {
        auto& ls = *reinterpret_cast<const List*>(obj);
        s->begin_sequence(ls.size());
        for (const auto& val : ls)
        {
            s->write_value(val);
        }
        s->end_sequence();
    }

    void deserialize(void* obj, deserializer* d) const
    {
        auto& ls = *reinterpret_cast<List*>(obj);
        size_t ls_size = d->begin_sequence();
        for (size_t i = 0; i < ls_size; ++i)
        {
            pt_value val = d->read_value(vptype);
            ls.push_back(std::move(pt_value_cast<value_type&>(val)));
        }
        d->end_sequence();
    }

    void* new_instance() const
    {
        return new List();
    }

    void delete_instance(void* ptr) const
    {
        delete reinterpret_cast<List*>(ptr);
    }

};

/**
 * @brief {@link meta_type} implementation for std::pair.
 */
template<typename T1, typename T2>
class pair_member : public meta_type
{

    static constexpr primitive_type ptype1 = type_to_ptype<T1>::ptype;
    static constexpr primitive_type ptype2 = type_to_ptype<T2>::ptype;

    static_assert(ptype1 != pt_null, "T1 is not a primitive type");
    static_assert(ptype2 != pt_null, "T2 is not a primitive type");

    typedef std::pair<T1, T2> pair_type;

 public:

    void serialize(const void* obj, serializer* s) const
    {
        auto& p = *reinterpret_cast<const pair_type*>(obj);
        pt_value values[2] = { p.first, p.second };
        s->write_tuple(2, values);
    }

    void deserialize(void* obj, deserializer* d) const
    {
        primitive_type ptypes[2] = { ptype1, ptype2 };
        pt_value values[2];
        d->read_tuple(2, ptypes, values);
        auto& p = *reinterpret_cast<pair_type*>(obj);
        p.first = std::move(pt_value_cast<T1&>(values[0]));
        p.second = std::move(pt_value_cast<T2&>(values[1]));
    }

    void* new_instance() const
    {
        return new pair_type();
    }

    void delete_instance(void* ptr) const
    {
        delete reinterpret_cast<pair_type*>(ptr);
    }

};

// matches value_type of std::set
template<typename T>
struct meta_value_type
{
    primitive_member<T> impl;
    void serialize_value(const T& what, serializer* s) const
    {
        impl.serialize(&what, s);
    }
    template<typename M>
    void deserialize_and_insert(M& map, deserializer* d) const
    {
        T value;
        impl.deserialize(&value, d);
        map.insert(std::move(value));
    }
};

// matches value_type of std::map
template<typename T1, typename T2>
struct meta_value_type<std::pair<const T1, T2>>
{
    pair_member<T1, T2> impl;
    void serialize_value(const std::pair<const T1, T2>& what, serializer* s) const
    {
        std::pair<T1, T2> p(what.first, what.second);
        impl.serialize(&p, s);
    }
    template<typename M>
    void deserialize_and_insert(M& map, deserializer* d) const
    {
        std::pair<T1, T2> p;
        impl.deserialize(&p, d);
        std::pair<const T1, T2> v(std::move(p.first), std::move(p.second));
        map.insert(std::move(v));
    }
};

/**
 * @brief {@link meta_type} implementation for STL compliant
 *        maps (such as std::map and std::set).
 *
 * This implementation requires primitive key and value types
 * (or a pair of primitive types as value type).
 */
template<typename Map>
class map_member : public meta_type
{

    typedef typename Map::key_type key_type;
    typedef typename Map::value_type value_type;

    meta_value_type<value_type> m_value_type_meta;

 public:

    void serialize(const void* obj, serializer* s) const
    {
        auto& mp = *reinterpret_cast<const Map*>(obj);
        s->begin_sequence(mp.size());
        for (const auto& val : mp)
        {
            m_value_type_meta.serialize_value(val, s);
        }
        s->end_sequence();
    }

    void deserialize(void* obj, deserializer* d) const
    {
        auto& mp = *reinterpret_cast<Map*>(obj);
        size_t mp_size = d->begin_sequence();
        for (size_t i = 0; i < mp_size; ++i)
        {
            m_value_type_meta.deserialize_and_insert(mp, d);
        }
        d->end_sequence();
    }

    void* new_instance() const
    {
        return new Map();
    }

    void delete_instance(void* ptr) const
    {
        delete reinterpret_cast<Map*>(ptr);
    }

};

/**
 * @brief {@link meta_type} implementation for user-defined structs.
 */
template<class Struct>
class meta_struct : public meta_type
{

    template<typename T>
    struct is_invalid
    {
        static const bool value =    !is_primitive<T>::value
                                  && !has_push_back<T>::value
                                  && !has_insert<T>::value;
    };

    class member
    {

        meta_type* m_meta;

        std::function<void (const meta_type*,
                            const void*,
                            serializer*      )> m_serialize;

        std::function<void (const meta_type*,
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
        member(meta_type* mtptr, S&& s, D&& d)
            : m_meta(mtptr)
            , m_serialize(std::forward<S>(s))
            , m_deserialize(std::forward<D>(d))
        {
        }

     public:

        template<typename T, class C>
        member(meta_type* mtptr, T C::*mem_ptr) : m_meta(mtptr)
        {
            m_serialize = [mem_ptr] (const meta_type* mt,
                                     const void* obj,
                                     serializer* s)
            {
                mt->serialize(&(*reinterpret_cast<const C*>(obj).*mem_ptr), s);
            };
            m_deserialize = [mem_ptr] (const meta_type* mt,
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
        static member fake_member(meta_type* mtptr)
        {
            return {
                mtptr,
                [] (const meta_type* mt, const void* obj, serializer* s)
                {
                    mt->serialize(obj, s);
                },
                [] (const meta_type* mt, void* obj, deserializer* d)
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

    std::string class_name;
    std::vector<member> m_members;

    // terminates recursion
    inline void push_back() { }

    // pr.first = member pointer
    // pr.second = meta object to handle pr.first
    template<typename T, class C, typename... Args>
    void push_back(std::pair<T C::*, meta_struct<T>*> pr, const Args&... args)
    {
        m_members.push_back({ pr.second, pr.first });
        push_back(args...);
    }

    template<class C, typename T, typename... Args>
    typename enable_if<is_primitive<T> >::type
    push_back(T C::*mem_ptr, const Args&... args)
    {
        m_members.push_back({ new primitive_member<T>(), mem_ptr });
        push_back(args...);
    }

    template<class C, typename T, typename... Args>
    typename enable_if<has_push_back<T> >::type
    push_back(T C::*mem_ptr, const Args&... args)
    {
        m_members.push_back({ new list_member<T>(), mem_ptr });
        push_back(args...);
    }

    template<class C, typename T, typename... Args>
    typename enable_if<has_insert<T> >::type
    push_back(T C::*mem_ptr, const Args&... args)
    {
        m_members.push_back({ new map_member<T>(), mem_ptr });
        push_back(args...);
    }

    template<class C, typename T, typename... Args>
    typename enable_if<is_invalid<T>>::type
    push_back(T C::*mem_ptr, const Args&... args)
    {
        static_assert(is_primitive<T>::value,
                      "T is neither a primitive type nor a list nor a map");
    }

    template<typename T>
    void init_(typename enable_if<is_primitive<T>>::type* = nullptr)
    {
        m_members.push_back(member::fake_member(new primitive_member<T>()));
    }

    template<typename T>
    void init_(typename disable_if<is_primitive<T>>::type* = nullptr)
    {
        static_assert(is_primitive<T>::value,
                      "T is neither a primitive type nor a list nor a map");
    }

 public:

    template<typename... Args>
    meta_struct(const Args&... args)
        : class_name(cppa::detail::to_uniform_name(typeid(Struct)))
    {
        push_back(args...);
    }

    meta_struct() : class_name(cppa::detail::to_uniform_name(typeid(Struct)))
    {
        init_<Struct>();
    }

    void serialize(const void* obj, serializer* s) const
    {
        s->begin_object(class_name);
        for (auto& m : m_members)
        {
            m.serialize(obj, s);
        }
        s->end_object();
    }

    void deserialize(void* obj, deserializer* d) const
    {
        std::string cname = d->seek_object();
        if (cname != class_name)
        {
            throw std::logic_error("wrong type name found");
        }
        d->begin_object(class_name);
        for (auto& m : m_members)
        {
            m.deserialize(obj, d);
        }
        d->end_object();
    }

    void* new_instance() const
    {
        return new Struct();
    }

    void delete_instance(void* ptr) const
    {
        delete reinterpret_cast<Struct*>(ptr);
    }

};

struct struct_a
{
    int x;
    int y;
};

bool operator==(const struct_a& lhs, const struct_a& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool operator!=(const struct_a& lhs, const struct_a& rhs)
{
    return !(lhs == rhs);
}

struct struct_b
{
    struct_a a;
    int z;
    std::list<int> ints;
};

bool operator==(const struct_b& lhs, const struct_b& rhs)
{
    return lhs.a == rhs.a && lhs.z == rhs.z && lhs.ints == rhs.ints;
}

bool operator!=(const struct_b& lhs, const struct_b& rhs)
{
    return !(lhs == rhs);
}

struct struct_c
{
    std::map<std::string, std::u16string> strings;
    std::set<int> ints;
};

bool operator==(const struct_c& lhs, const struct_c& rhs)
{
    return lhs.strings == rhs.strings && lhs.ints == rhs.ints;
}

bool operator!=(const struct_c& lhs, const struct_c& rhs)
{
    return !(lhs == rhs);
}

class string_serializer : public serializer
{

    std::ostream& out;

    struct pt_writer
    {

        std::ostream& out;

        pt_writer(std::ostream& mout) : out(mout) { }

        template<typename T>
        void operator()(const T& value)
        {
            out << value;
        }

        void operator()(const std::string& str)
        {
            out << "\"" << str << "\"";
        }

        void operator()(const std::u16string&) { }

        void operator()(const std::u32string&) { }

    };

    int m_open_objects;
    bool m_after_value;

    inline void clear()
    {
        if (m_after_value)
        {
            out << ", ";
            m_after_value = false;
        }
    }

 public:

    string_serializer(std::ostream& mout)
        : out(mout), m_open_objects(0), m_after_value(false) { }

    void begin_object(const std::string& type_name)
    {
        clear();
        ++m_open_objects;
        out << type_name << " ( ";
    }
    void end_object()
    {
        out << " )";
    }

    void begin_sequence(size_t)
    {
        clear();
        out << "{ ";
    }

    void end_sequence()
    {
        out << (m_after_value ? " }" : "}");
    }

    void write_value(const pt_value& value)
    {
        clear();
        value.apply(pt_writer(out));
        m_after_value = true;
    }

    void write_tuple(size_t size, const pt_value* values)
    {
        clear();
        out << " {";
        const pt_value* end = values + size;
        for ( ; values != end; ++values)
        {
            write_value(*values);
        }
        out << (m_after_value ? " }" : "}");
    }

};

/*
class xml_serializer : public serializer
{

    std::ostream& out;
    std::string indentation;

    inline void inc_indentation()
    {
        indentation.resize(indentation.size() + 4, ' ');
    }

    inline void dec_indentation()
    {
        auto isize = indentation.size();
        indentation.resize((isize > 4) ? (isize - 4) : 0);
    }

    struct pt_writer
    {

        std::ostream& out;
        const std::string& indentation;

        pt_writer(std::ostream& ostr, const std::string& indent)
            : out(ostr), indentation(indent)
        {
        }

        template<typename T>
        void operator()(const T& value)
        {
            out << indentation << "<value type=\""
                << primitive_type_name(type_to_ptype<T>::ptype)
                << "\">" << value << "</value>\n";
        }

        void operator()(const std::u16string&) { }

        void operator()(const std::u32string&) { }

    };

 public:

    xml_serializer(std::ostream& ostr) : out(ostr), indentation("") { }

    void begin_object(const std::string& type_name)
    {
        out << indentation << "<object type=\"" << type_name << "\">\n";
        inc_indentation();
    }
    void end_object()
    {
        dec_indentation();
        out << indentation << "</object>\n";
    }

    void begin_list(size_t)
    {
        out << indentation << "<list>\n";
        inc_indentation();
    }

    void end_list()
    {
        dec_indentation();
        out << indentation << "</list>\n";
    }

    void write_value(const pt_value& value)
    {
        value.apply(pt_writer(out, indentation));
    }

};
*/

class binary_serializer : public serializer
{

    typedef char* buf_type;

    buf_type m_buf;
    size_t m_wr_pos;

    template<typename T>
    void write(const T& value)
    {
        memcpy(m_buf + m_wr_pos, &value, sizeof(T));
        m_wr_pos += sizeof(T);
    }

    void write(const std::string& str)
    {
        write(static_cast<std::uint32_t>(str.size()));
        memcpy(m_buf + m_wr_pos, str.c_str(), str.size());
        m_wr_pos += str.size();
    }

    void write(const std::u16string& str)
    {
        write(static_cast<std::uint32_t>(str.size()));
        for (char16_t c : str)
        {
            write(static_cast<std::uint16_t>(c));
        }
    }

    void write(const std::u32string& str)
    {
        write(static_cast<std::uint32_t>(str.size()));
        for (char32_t c : str)
        {
            write(static_cast<std::uint32_t>(c));
        }
    }

    struct pt_writer
    {

        binary_serializer* self;

        inline pt_writer(binary_serializer* mself) : self(mself) { }

        template<typename T>
        inline void operator()(const T& value)
        {
            self->write(value);
        }

    };

 public:

    binary_serializer(char* buf) : m_buf(buf), m_wr_pos(0) { }

    void begin_object(const std::string& tname)
    {
        write(tname);
    }

    void end_object()
    {
    }

    void begin_sequence(size_t list_size)
    {
        write(static_cast<std::uint32_t>(list_size));
    }

    void end_sequence()
    {
    }

    void write_value(const pt_value& value)
    {
        value.apply(pt_writer(this));
    }

    void write_tuple(size_t size, const pt_value* values)
    {
        const pt_value* end = values + size;
        for ( ; values != end; ++values)
        {
            write_value(*values);
        }
    }

};

class binary_deserializer : public deserializer
{

    typedef char* buf_type;

    buf_type m_buf;
    size_t m_rd_pos;
    size_t m_buf_size;

    void range_check(size_t read_size)
    {
        if (m_rd_pos + read_size >= m_buf_size)
        {
            std::out_of_range("binary_deserializer::read()");
        }
    }

    template<typename T>
    void read(T& storage, bool modify_rd_pos = true)
    {
        range_check(sizeof(T));
        memcpy(&storage, m_buf + m_rd_pos, sizeof(T));
        if (modify_rd_pos) m_rd_pos += sizeof(T);
    }

    void read(std::string& str, bool modify_rd_pos = true)
    {
        std::uint32_t str_size;
        read(str_size, modify_rd_pos);
        char* cpy_begin;
        if (modify_rd_pos)
        {
            range_check(str_size);
            cpy_begin = m_buf + m_rd_pos;
        }
        else
        {
            range_check(sizeof(std::uint32_t) + str_size);
            cpy_begin = m_buf + m_rd_pos + sizeof(std::uint32_t);
        }
        str.clear();
        str.reserve(str_size);
        char* cpy_end = cpy_begin + str_size;
        std::copy(cpy_begin, cpy_end, std::back_inserter(str));
        if (modify_rd_pos) m_rd_pos += str_size;
    }

    template<typename CharType, typename StringType>
    void read_unicode_string(StringType& str)
    {
        std::uint32_t str_size;
        read(str_size);
        str.reserve(str_size);
        for (size_t i = 0; i < str_size; ++i)
        {
            CharType c;
            read(c);
            str += static_cast<typename StringType::value_type>(c);
        }
    }

    void read(std::u16string& str)
    {
        // char16_t is guaranteed to has *at least* 16 bytes,
        // but not to have *exactly* 16 bytes; thus use uint16_t
        read_unicode_string<std::uint16_t>(str);
    }

    void read(std::u32string& str)
    {
        // char32_t is guaranteed to has *at least* 32 bytes,
        // but not to have *exactly* 32 bytes; thus use uint32_t
        read_unicode_string<std::uint32_t>(str);
    }

    struct pt_reader
    {
        binary_deserializer* self;
        inline pt_reader(binary_deserializer* mself) : self(mself) { }
        template<typename T>
        inline void operator()(T& value)
        {
            self->read(value);
        }
    };

 public:

    binary_deserializer(buf_type buf, size_t buf_size)
        : m_buf(buf), m_rd_pos(0), m_buf_size(buf_size)
    {
    }

    std::string seek_object()
    {
        std::string result;
        read(result);
        return result;
    }

    std::string peek_object()
    {
        std::string result;
        read(result, false);
        return result;
    }

    void begin_object(const std::string&)
    {
    }

    void end_object()
    {
    }

    size_t begin_sequence()
    {
        std::uint32_t size;
        read(size);
        return size;
    }

    void end_sequence()
    {
    }

    pt_value read_value(primitive_type ptype)
    {
        pt_value val(ptype);
        val.apply(pt_reader(this));
        return val;
    }

    void read_tuple(size_t size,
                    const primitive_type* ptypes,
                    pt_value* storage)
    {
        const primitive_type* end = ptypes + size;
        for ( ; ptypes != end; ++ptypes)
        {
            *storage = std::move(read_value(*ptypes));
            ++storage;
        }
    }

};

class string_deserializer : public deserializer
{

    std::string m_str;
    std::string::iterator m_pos;
    size_t m_obj_count;

    void skip_space_and_comma()
    {
        while (*m_pos == ' ' || *m_pos == ',') ++m_pos;
    }

    void throw_malformed(const std::string& error_msg)
    {
        throw std::logic_error("malformed string: " + error_msg);
    }

    void consume(char c)
    {
        skip_space_and_comma();
        if (*m_pos != c)
        {
            std::string error;
            error += "expected '";
            error += c;
            error += "' found '";
            error += *m_pos;
            error += "'";
            throw_malformed(error);
        }
        ++m_pos;
    }

    inline std::string::iterator next_delimiter()
    {
        return std::find_if(m_pos, m_str.end(), [] (char c) -> bool {
            switch (c)
            {
             case '(':
             case ')':
             case '{':
             case '}':
             case ' ':
             case ',': return true;
             default : return false;
            }
        });
    }

 public:

    string_deserializer(const std::string& str) : m_str(str)
    {
        m_pos = m_str.begin();
        m_obj_count = 0;
    }

    string_deserializer(std::string&& str) : m_str(std::move(str))
    {
        m_pos = m_str.begin();
        m_obj_count = 0;
    }

    std::string seek_object()
    {
        skip_space_and_comma();
        auto substr_end = next_delimiter();
        // next delimiter must eiter be '(' or "\w+\("
        if (substr_end == m_str.end() ||  *substr_end != '(')
        {
            auto peeker = substr_end;
            while (peeker != m_str.end() && *peeker == ' ') ++peeker;
            if (peeker == m_str.end() ||  *peeker != '(')
            {
                throw_malformed("type name not followed by '('");
            }
        }
        std::string result(m_pos, substr_end);
        m_pos = substr_end;
        return result;
    }

    std::string peek_object()
    {
        std::string result = seek_object();
        // restore position in stream
        m_pos -= result.size();
        return result;
    }

    void begin_object(const std::string&)
    {
        ++m_obj_count;
        skip_space_and_comma();
        consume('(');
    }

    void end_object()
    {
        consume(')');
        if (--m_obj_count == 0)
        {
            skip_space_and_comma();
            if (m_pos != m_str.end())
            {
                throw_malformed("expected end of of string");
            }
        }
    }

    size_t begin_sequence()
    {
        consume('{');
        auto list_end = std::find(m_pos, m_str.end(), '}');
        return std::count(m_pos, list_end, ',') + 1;
    }

    void end_sequence()
    {
        consume('}');
    }

    struct from_string
    {
        const std::string& str;
        from_string(const std::string& s) : str(s) { }
        template<typename T>
        void operator()(T& what)
        {
            std::istringstream s(str);
            s >> what;
        }
        void operator()(std::string& what)
        {
            what = str;
        }
        void operator()(std::u16string&) { }
        void operator()(std::u32string&) { }
    };

    pt_value read_value(primitive_type ptype)
    {
        skip_space_and_comma();
        auto substr_end = std::find_if(m_pos, m_str.end(), [] (char c) -> bool {
            switch (c)
            {
             case ')':
             case '}':
             case ' ':
             case ',': return true;
             default : return false;
            }
        });
        std::string substr(m_pos, substr_end);
        pt_value result(ptype);
        result.apply(from_string(substr));
        m_pos += substr.size();
        return result;
    }

    void foo() {}

    void read_tuple(size_t size, const primitive_type* begin, pt_value* storage)
    {
        consume('{');
        const primitive_type* end = begin + size;
        for ( ; begin != end; ++begin)
        {
            *storage = std::move(read_value(*begin));
            ++storage;
        }
        consume('}');
    }

};

std::string to_string(void* what, meta_type* mobj)
{
    std::ostringstream osstr;
    string_serializer strs(osstr);
    mobj->serialize(what, &strs);
    return osstr.str();
}

template<class C, class Parent, typename... Args>
std::pair<C Parent::*, meta_struct<C>*>
compound_member(C Parent::*c_ptr, const Args&... args)
{
    return std::make_pair(c_ptr, new meta_struct<C>(args...));
}

template<class C, typename... Args>
meta_type* meta_object(const Args&... args)
{
    return new meta_struct<C>(args...);
}



std::size_t test__serialization()
{
    CPPA_TEST(test__serialization);

    CPPA_CHECK_EQUAL((is_iterable<int>::value), false);
    // std::string is primitive and thus not identified by is_iterable
    CPPA_CHECK_EQUAL((is_iterable<std::string>::value), false);
    CPPA_CHECK_EQUAL((is_iterable<std::list<int>>::value), true);
    CPPA_CHECK_EQUAL((is_iterable<std::map<int,int>>::value), true);
    // test pt_value implementation
    {
        pt_value v1(42);
        pt_value v2(42);
        CPPA_CHECK_EQUAL(v1, v2);
        CPPA_CHECK_EQUAL(v1, 42);
        CPPA_CHECK_EQUAL(42, v2);
        // type mismatch => unequal
        CPPA_CHECK(v2 != static_cast<std::int8_t>(42));
    }
    root_object root;
    // test meta_struct implementation for primitive types
    {
        meta_struct<std::uint32_t> meta_int;
        auto i = meta_int.new_instance();
        auto str = to_string(i, &meta_int);
        meta_int.delete_instance(i);
        cout << "str: " << str << endl;
    }
    // test serializers / deserializers with struct_b
    {
        // get meta object for struct_b
        auto meta_b = meta_object<struct_b>(compound_member(&struct_b::a,
                                                            &struct_a::x,
                                                            &struct_a::y),
                                            &struct_b::z,
                                            &struct_b::ints);
        // "announce" meta types
        s_meta_types["struct_b"] = meta_b;
        // testees
        struct_b b1 = { { 1, 2 }, 3, { 4, 5, 6, 7, 8, 9, 10 } };
        struct_b b2;
        struct_b b3;
        // expected result of to_string(&b1, meta_b)
        auto b1str = "struct_b ( struct_a ( 1, 2 ), 3, "
                     "{ 4, 5, 6, 7, 8, 9, 10 } )";
        // verify
        CPPA_CHECK_EQUAL((to_string(&b1, meta_b)), b1str);
        // binary buffer
        char buf[512];
        // serialize b1 to buf
        {
            binary_serializer bs(buf);
            meta_b->serialize(&b1, &bs);
        }
        // deserialize b2 from buf
        {
            binary_deserializer bd(buf, 512);
            auto res = root.deserialize(&bd);
            CPPA_CHECK_EQUAL(res.second, meta_b);
            if (res.second == meta_b)
            {
                b2 = *reinterpret_cast<struct_b*>(res.first);
            }
            res.second->delete_instance(res.first);
        }
        // verify result of serialization / deserialization
        CPPA_CHECK_EQUAL(b1, b2);
        CPPA_CHECK_EQUAL((to_string(&b2, meta_b)), b1str);
        // deserialize b3 from string, using string_deserializer
        {
            string_deserializer strd(b1str);
            auto res = root.deserialize(&strd);
            CPPA_CHECK_EQUAL(res.second, meta_b);
            if (res.second == meta_b)
            {
                b3 = *reinterpret_cast<struct_b*>(res.first);
            }
            res.second->delete_instance(res.first);
        }
        CPPA_CHECK_EQUAL(b1, b3);
        // cleanup
        s_meta_types.clear();
        delete meta_b;
    }
    // test serializers / deserializers with struct_c
    {
        // get meta type of struct_c and "announce"
        auto meta_c = meta_object<struct_c>(&struct_c::strings,&struct_c::ints);
        s_meta_types["struct_c"] = meta_c;
        // testees
        struct_c c1 = { { { "abc", u"cba" }, { "x", u"y" } }, { 9, 4, 5 } };
        struct_c c2;
        // binary buffer
        char buf[512];
        // serialize c1 to buf
        {
            binary_serializer bs(buf);
            meta_c->serialize(&c1, &bs);
        }
        // serialize c2 from buf
        {
            binary_deserializer bd(buf, 512);
            auto res = root.deserialize(&bd);
            CPPA_CHECK_EQUAL(res.second, meta_c);
            if (res.second == meta_c)
            {
                c2 = *reinterpret_cast<struct_c*>(res.first);
            }
            res.second->delete_instance(res.first);
        }
        // verify result of serialization / deserialization
        CPPA_CHECK_EQUAL(c1, c2);
    }
    return CPPA_TEST_RESULT;
}
