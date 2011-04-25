#include <new>
#include <locale>
#include <memory>
#include <string>
#include <limits>
#include <vector>
#include <cstring>
#include <sstream>
#include <cstdint>
#include <typeinfo>
#include <iostream>
#include <algorithm>
#include <type_traits>
#include <cassert>

#include "test.hpp"

#include "cppa/util.hpp"
#include "cppa/match.hpp"
#include "cppa/tuple.hpp"
#include "cppa/serializer.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/untyped_tuple.hpp"
#include "cppa/util/enable_if.hpp"

#include "cppa/util.hpp"

using std::cout;
using std::cerr;
using std::endl;

/**
 * @brief Integers, floating points and strings.
 */
enum fundamental_type
{
    ft_int8,        ft_int16,       ft_int32,        ft_int64,
    ft_uint8,       ft_uint16,      ft_uint32,       ft_uint64,
    ft_float,       ft_double,      ft_long_double,
    ft_u8string,    ft_u16string,   ft_u32string,
    ft_null
};

constexpr const char* fundamental_type_names[] =
{
    "ft_int8",        "ft_int16",       "ft_int32",       "ft_int64",
    "ft_uint8",       "ft_uint16",      "ft_uint32",      "ft_uint64",
    "ft_float",       "ft_double",      "ft_long_double",
    "ft_u8string",    "ft_u16string",   "ft_u32string",
    "ft_null"
};

constexpr const char* fundamental_type_name(fundamental_type ftype)
{
    return fundamental_type_names[static_cast<int>(ftype)];
}

// achieves static call dispatch (Int-To-Type idiom)
template<fundamental_type FT>
struct ft_token { static const fundamental_type value = FT; };

// if (IfStmt == true) type = T; else type = Else::type;
template<bool IfStmt, typename T, class Else>
struct if_else_type_c
{
    typedef T type;
};

template<typename T, class Else>
struct if_else_type_c<false, T, Else>
{
    typedef typename Else::type type;
};

// if (Stmt::value == true) type = T; else type = Else::type;
template<class Stmt, typename T, class Else>
struct if_else_type : if_else_type_c<Stmt::value, T, Else> { };

template<typename T>
struct wrapped_type { typedef T type; };

// maps the fundamental_type FT to the corresponding type
template<fundamental_type FT>
struct ftype_to_type
  : if_else_type_c<FT == ft_int8, std::int8_t,
    if_else_type_c<FT == ft_int16, std::int16_t,
    if_else_type_c<FT == ft_int32, std::int32_t,
    if_else_type_c<FT == ft_int64, std::int64_t,
    if_else_type_c<FT == ft_uint8, std::uint8_t,
    if_else_type_c<FT == ft_uint16, std::uint16_t,
    if_else_type_c<FT == ft_uint32, std::uint32_t,
    if_else_type_c<FT == ft_uint64, std::uint64_t,
    if_else_type_c<FT == ft_float, float,
    if_else_type_c<FT == ft_double, double,
    if_else_type_c<FT == ft_long_double, long double,
    if_else_type_c<FT == ft_u8string, std::string,
    if_else_type_c<FT == ft_u16string, std::u16string,
    if_else_type_c<FT == ft_u32string, std::u32string,
    wrapped_type<void> > > > > > > > > > > > > > >
{
};

// if (IfStmt == true) ftype = FT; else ftype = Else::ftype;
template<bool IfStmt, fundamental_type FT, class Else>
struct if_else_ftype_c
{
    static const fundamental_type ftype = FT;
};

template<fundamental_type FT, class Else>
struct if_else_ftype_c<false, FT, Else>
{
    static const fundamental_type ftype = Else::ftype;
};

// if (Stmt::value == true) ftype = FT; else ftype = Else::ftype;
template<class Stmt, fundamental_type FT, class Else>
struct if_else_ftype : if_else_ftype_c<Stmt::value, FT, Else> { };

template<fundamental_type FT>
struct wrapped_ftype { static const fundamental_type ftype = FT; };

// maps type T the the corresponding fundamental_type
template<typename T>
struct type_to_ftype
      // signed integers
    : if_else_ftype<std::is_same<T, std::int8_t>, ft_int8,
      if_else_ftype<std::is_same<T, std::int16_t>, ft_int16,
      if_else_ftype<std::is_same<T, std::int32_t>, ft_int32,
      if_else_ftype<std::is_same<T, std::int64_t>, ft_int64,
      if_else_ftype<std::is_same<T, std::uint8_t>, ft_uint8,
      // unsigned integers
      if_else_ftype<std::is_same<T, std::uint16_t>, ft_uint16,
      if_else_ftype<std::is_same<T, std::uint32_t>, ft_uint32,
      if_else_ftype<std::is_same<T, std::uint64_t>, ft_uint64,
      // float / double
      if_else_ftype<std::is_same<T, float>, ft_float,
      if_else_ftype<std::is_same<T, double>, ft_double,
      if_else_ftype<std::is_same<T, long double>, ft_long_double,
      // strings
      if_else_ftype<std::is_convertible<T, std::string>, ft_u8string,
      if_else_ftype<std::is_convertible<T, std::u16string>, ft_u16string,
      if_else_ftype<std::is_convertible<T, std::u32string>, ft_u32string,
      wrapped_ftype<ft_null> > > > > > > > > > > > > > >
{
};

template<typename T>
struct type_to_ftype<T&> : type_to_ftype<T> { };

template<typename T>
struct type_to_ftype<T&&> : type_to_ftype<T> { };

template<typename T>
struct type_to_ftype<const T&> : type_to_ftype<T> { };

template<typename T>
struct type_to_ftype<const T> : type_to_ftype<T> { };

struct value
{
    fundamental_type type;
    inline value(const fundamental_type& ftype) : type(ftype) { }
};

struct list
{
    fundamental_type value_type;
    inline list(const fundamental_type& ftype) : value_type(ftype) { }
};

struct map
{
    fundamental_type key_type;
    fundamental_type value_type;
    inline map(const fundamental_type& key_ft, const fundamental_type& value_ft)
        : key_type(key_ft), value_type(value_ft)
    {
    }
};

using cppa::util::enable_if;

template<bool Stmt, typename T>
struct disable_if_c { };

template<typename T>
struct disable_if_c<false, T>
{
    typedef T type;
};

template<class Trait, typename T = void>
struct disable_if : disable_if_c<Trait::value, T> { };

template<fundamental_type FT, class T, class V>
void set_fun(fundamental_type& lhs_type, T& lhs, V&& rhs,
             typename disable_if<std::is_arithmetic<T>>::type* = nullptr)
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

template<fundamental_type FT, class T, class V>
void set_fun(fundamental_type& lhs_type, T& lhs, V&& rhs,
             typename enable_if<std::is_arithmetic<T>>::type* = nullptr)
{
    lhs = rhs;
    lhs_type = FT;
}

template<class T>
void destroy_fun(T& what,
                 typename disable_if<std::is_arithmetic<T>>::type* = nullptr)
{
    what.~T();
}

template<class T>
void destroy_fun(T&, typename enable_if<std::is_arithmetic<T>>::type* = nullptr)
{
    // arithmetic types don't need destruction
}

class ft_value;

template<typename T>
T ft_value_cast(ft_value&);

template<typename T>
T ft_value_cast(const ft_value&);

template<fundamental_type FT>
typename ftype_to_type<FT>::type& ft_value_cast(ft_value&);

template<fundamental_type FT>
const typename ftype_to_type<FT>::type& ft_value_cast(const ft_value&);

template<typename Fun>
void ft_invoke(fundamental_type ftype, Fun&& f)
{
    switch (ftype)
    {
     case ft_int8:        f(ft_token<ft_int8>());        break;
     case ft_int16:       f(ft_token<ft_int16>());       break;
     case ft_int32:       f(ft_token<ft_int32>());       break;
     case ft_int64:       f(ft_token<ft_int64>());       break;
     case ft_uint8:       f(ft_token<ft_uint8>());       break;
     case ft_uint16:      f(ft_token<ft_uint16>());      break;
     case ft_uint32:      f(ft_token<ft_uint32>());      break;
     case ft_uint64:      f(ft_token<ft_uint64>());      break;
     case ft_float:       f(ft_token<ft_float>());       break;
     case ft_double:      f(ft_token<ft_double>());      break;
     case ft_long_double: f(ft_token<ft_long_double>()); break;
     case ft_u8string:    f(ft_token<ft_u8string>());    break;
     case ft_u16string:   f(ft_token<ft_u16string>());   break;
     case ft_u32string:   f(ft_token<ft_u32string>());   break;
     default: break;
    }
}

/**
 * @brief Describes a value of a {@link fundamental_type}.
 */
class ft_value
{

    template<typename T>
    friend T ft_value_cast(ft_value&);

    template<typename T>
    friend T ft_value_cast(const ft_value&);


    template<fundamental_type FT>
    friend typename ftype_to_type<FT>::type& ft_value_cast(ft_value&);

    template<fundamental_type FT>
    friend const typename ftype_to_type<FT>::type& ft_value_cast(const ft_value&);

    fundamental_type m_ftype;

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
    inline decltype(i8)&    get(ft_token<ft_int8>)        { return i8;    }
    inline decltype(i16)&   get(ft_token<ft_int16>)       { return i16;   }
    inline decltype(i32)&   get(ft_token<ft_int32>)       { return i32;   }
    inline decltype(i64)&   get(ft_token<ft_int64>)       { return i64;   }
    inline decltype(u8)&    get(ft_token<ft_uint8>)       { return u8;    }
    inline decltype(u16)&   get(ft_token<ft_uint16>)      { return u16;   }
    inline decltype(u32)&   get(ft_token<ft_uint32>)      { return u32;   }
    inline decltype(u64)&   get(ft_token<ft_uint64>)      { return u64;   }
    inline decltype(fl)&    get(ft_token<ft_float>)       { return fl;    }
    inline decltype(dbl)&   get(ft_token<ft_double>)      { return dbl;   }
    inline decltype(ldbl)&  get(ft_token<ft_long_double>) { return ldbl;  }
    inline decltype(str8)&  get(ft_token<ft_u8string>)    { return str8;  }
    inline decltype(str16)& get(ft_token<ft_u16string>)   { return str16; }
    inline decltype(str32)& get(ft_token<ft_u32string>)   { return str32; }

    // get(...) const overload
    template<fundamental_type FT>
    const typename ftype_to_type<FT>::type& get(ft_token<FT> token) const
    {
        return const_cast<ft_value*>(this)->get(token);
    }

    struct destroyer
    {
        ft_value* m_self;
        inline destroyer(ft_value* self) : m_self(self) { }
        template<fundamental_type FT>
        inline void operator()(ft_token<FT> token) const
        {
            destroy_fun(m_self->get(token));
        }
    };

    struct initializer
    {
        ft_value* m_self;
        inline initializer(ft_value* self) : m_self(self) { }
        template<fundamental_type FT>
        inline void operator()(ft_token<FT> token) const
        {
            set_fun<FT>(m_self->m_ftype,
                        m_self->get(token),
                        typename ftype_to_type<FT>::type());
        }
    };

    struct setter
    {
        ft_value* m_self;
        const ft_value& m_other;
        inline setter(ft_value* self, const ft_value& other)
            : m_self(self), m_other(other) { }
        template<fundamental_type FT>
        inline void operator()(ft_token<FT> token) const
        {
            set_fun<FT>(m_self->m_ftype,
                        m_self->get(token),
                        m_other.get(token));
        }
    };

    struct mover
    {
        ft_value* m_self;
        const ft_value& m_other;
        inline mover(ft_value* self, const ft_value& other)
            : m_self(self), m_other(other) { }
        template<fundamental_type FT>
        inline void operator()(ft_token<FT> token) const
        {
            set_fun<FT>(m_self->m_ftype,
                        m_self->get(token),
                        std::move(m_other.get(token)));
        }
    };

    struct comparator
    {
        bool m_result;
        const ft_value* m_self;
        const ft_value& m_other;
        inline comparator(const ft_value* self, const ft_value& other)
            : m_result(false), m_self(self), m_other(other) { }
        template<fundamental_type FT>
        inline void operator()(ft_token<FT> token)
        {
            if (m_other.m_ftype == FT)
            {
                m_result = (m_self->get(token) == m_other.get(token));
            }
        }
        inline bool result() const { return m_result; }
    };

    void destroy()
    {
        ft_invoke(m_ftype, destroyer(this));
        m_ftype = ft_null;
    }

    template<class Self, typename Fun>
    struct forwarder
    {
        Self* m_self;
        Fun& m_f;
        forwarder(Self* self, Fun& f) : m_self(self), m_f(f) { }
        template<fundamental_type FT>
        inline void operator()(ft_token<FT> token)
        {
            m_f(m_self->get(token));
        }
    };

 public:

    template<typename Fun>
    void apply(Fun&& f)
    {
        ft_invoke(m_ftype, forwarder<ft_value, Fun>(this, f));
    }

    template<typename Fun>
    void apply(Fun&& f) const
    {
        ft_invoke(m_ftype, forwarder<const ft_value, Fun>(this, f));
    }

    ft_value() : m_ftype(ft_null) { }

    template<typename V>
    ft_value(V&& value) : m_ftype(ft_null)
    {
        static_assert(type_to_ftype<V>::ftype != ft_null,
                      "T is not a valid value for ft_value");
        set_fun<type_to_ftype<V>::ftype>(m_ftype,
                                   get(ft_token<type_to_ftype<V>::ftype>()),
                                   std::forward<V>(value));
    }

    ft_value(fundamental_type ftype) : m_ftype(ft_null)
    {
        ft_invoke(ftype, initializer(this));
    }

    ft_value(const ft_value& other) : m_ftype(ft_null)
    {
        //invoke(setter(other));
        ft_invoke(other.m_ftype, setter(this, other));
    }

    ft_value(ft_value&& other) : m_ftype(ft_null)
    {
        //invoke(mover(other));
        ft_invoke(other.m_ftype, mover(this, other));
    }

    ft_value& operator=(const ft_value& other)
    {
        //invoke(setter(other));
        ft_invoke(other.m_ftype, setter(this, other));
        return *this;
    }

    ft_value& operator=(ft_value&& other)
    {
        //invoke(mover(other));
        ft_invoke(other.m_ftype, mover(this, other));
        return *this;
    }

    bool operator==(const ft_value& other) const
    {
        comparator cmp(this, other);
        ft_invoke(m_ftype, cmp);
        return cmp.result();
    }

    bool operator!=(const ft_value& other) const
    {
        return !(*this == other);
    }

    inline fundamental_type type() const { return m_ftype; }

    ~ft_value() { destroy(); }

};

template<typename T>
typename cppa::util::enable_if<std::is_arithmetic<T>, bool>::type
operator==(const T& lhs, const ft_value& rhs)
{
    return (rhs.type() == type_to_ftype<T>::ftype)
           ? lhs == ft_value_cast<const T&>(rhs)
           : false;
}

template<typename T>
typename cppa::util::enable_if<std::is_arithmetic<T>, bool>::type
operator==(const ft_value& lhs, const T& rhs)
{
    return (lhs.type() == type_to_ftype<T>::ftype)
           ? ft_value_cast<const T&>(lhs) == rhs
           : false;
}

template<typename T>
typename cppa::util::enable_if<std::is_arithmetic<T>, bool>::type
operator!=(const T& lhs, const ft_value& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
typename cppa::util::enable_if<std::is_arithmetic<T>, bool>::type
operator!=(const ft_value& lhs, const T& rhs)
{
    return !(lhs == rhs);
}

template<fundamental_type FT>
typename ftype_to_type<FT>::type& ft_value_cast(ft_value& v)
{
    if (v.type() != FT) throw std::bad_cast();
    return v.get(ft_token<FT>());
}

template<fundamental_type FT>
const typename ftype_to_type<FT>::type& ft_value_cast(const ft_value& v)
{
    if (v.type() != FT) throw std::bad_cast();
    return v.get(ft_token<FT>());
}

template<typename T>
T ft_value_cast(ft_value& v)
{
    static const fundamental_type ftype = type_to_ftype<T>::ftype;
    if (v.type() != ftype) throw std::bad_cast();
    return v.get(ft_token<ftype>());
}

template<typename T>
T ft_value_cast(const ft_value& v)
{
    typedef typename std::remove_reference<T>::type plain_t;
    static_assert(!std::is_reference<T>::value || std::is_const<plain_t>::value,
                  "Could not get a non-const reference from const ft_value&");
    static const fundamental_type ftype = type_to_ftype<T>::ftype;
    if (v.type() != ftype) throw std::bad_cast();
    return v.get(ft_token<ftype>());
}

class value_property
{

 public:

    virtual ~value_property() { }
    virtual void set(ft_value&& what) = 0;
    virtual void get(ft_value& storage) const = 0;
    virtual fundamental_type type() const = 0;

};

class list_property
{

 public:

    class iterator
    {

     public:

        virtual ~iterator() { }
        virtual void next() = 0;
        virtual bool at_end() const = 0;
        virtual ft_value get() const = 0;

    };

    virtual ~list_property() { }
    virtual size_t size() const = 0;
    virtual iterator* begin() const = 0;
    virtual fundamental_type value_type() const = 0;
    virtual void push_back(ft_value&& what) = 0;

};

class map_property
{

 public:

    class iterator
    {

     public:

        virtual ~iterator() { }
        virtual void next() = 0;
        virtual bool at_end() const = 0;
        virtual ft_value key() const = 0;
        virtual ft_value value() const = 0;

    };

    virtual ~map_property() { }
    virtual size_t size() const = 0;
    virtual iterator* begin() const = 0;
    virtual fundamental_type key_type() const = 0;
    virtual fundamental_type value_type() const = 0;
    virtual void insert(ft_value&& key, ft_value&& val) = 0;

};

template<typename Getter, typename Setter, fundamental_type FT>
class value_property_impl : public value_property
{

    Getter m_get;
    Setter m_set;
//    T* m_ptr;

 public:

//    value_property_impl(T* ptr) : m_ptr(ptr) { }

    value_property_impl(Getter g, Setter s) : m_get(g), m_set(s) { }

    void set(ft_value&& what)
    {
//        *m_ptr = std::move(ft_value_cast<FT>(what));
        m_set(std::move(ft_value_cast<FT>(what)));
    }

    void get(ft_value& storage) const
    {
//        ft_value_cast<FT>(storage) = *m_ptr;
        ft_value_cast<FT>(storage) = m_get();
    }

    fundamental_type type() const
    {
        return FT;
    }

};

template<class List, fundamental_type FT>
class list_property_impl : public list_property
{

    class iterator : public list_property::iterator
    {

        typedef typename List::const_iterator native_iterator;

        native_iterator pos;
        native_iterator end;

     public:

        iterator(native_iterator bg, native_iterator nd) : pos(bg), end(nd) { }

        bool at_end() const { return pos == end; }

        void next() { ++pos; }

        ft_value get() const { return *pos; }

    };

    List* m_list;

 public:

    list_property_impl(List* native_list) : m_list(native_list) { }

    size_t size() const { return m_list->size(); }

    list_property::iterator* begin() const
    {
        return new iterator(m_list->begin(), m_list->end());
    }

    void push_back(ft_value&& what)
    {
        m_list->push_back(std::move(ft_value_cast<FT>(what)));
    }

    fundamental_type value_type() const
    {
        return FT;
    }

};

template<class Map, fundamental_type KeyType, fundamental_type ValueType>
class map_property_impl : public map_property
{

    class iterator : public map_property::iterator
    {

        typedef typename Map::const_iterator native_iterator;

        native_iterator pos;
        native_iterator end;

     public:

        iterator(native_iterator bg, native_iterator nd) : pos(bg), end(nd) { }

        void next() { ++pos; }

        bool at_end() const { return pos == end; }

        ft_value key() const { return pos->first; }

        ft_value value() const { return pos->second; }

    };

    Map* m_map;

 public:

    map_property_impl(Map* ptr) : m_map(ptr) { }

    size_t size() const
    {
        return m_map->size();
    }

    iterator* begin() const
    {
        return new iterator(m_map->begin(), m_map->end());
    }

    void insert(ft_value&& k, ft_value&& v)
    {
        m_map->insert(std::make_pair(
                          std::move(ft_value_cast<KeyType>(k)),
                          std::move(ft_value_cast<ValueType>(v))));
    }

    fundamental_type key_type() const
    {
        return KeyType;
    }

    fundamental_type value_type() const
    {
        return ValueType;
    }

};

template<fundamental_type FT, typename Getter, typename Setter>
value_property* as_value_property(Getter getter, Setter setter)
{
    return new value_property_impl<Getter, Setter, FT>(getter, setter);
}

using cppa::util::enable_if;
using cppa::util::conjunction;

template<fundamental_type FT, class C, typename Getter, typename Setter>
typename enable_if<conjunction<std::is_member_function_pointer<Getter>,
                               std::is_member_function_pointer<Setter>>,
                   value_property*>::type
as_value_property(C* self, Getter getter, Setter setter)
{
    typedef cppa::util::callable_trait<Getter> getter_trait;
    typedef cppa::util::callable_trait<Setter> setter_trait;
    typedef typename getter_trait::result_type getter_result;
    typedef typename ftype_to_type<FT>::type setter_arg;
    return as_value_property<FT>(
                [=]() -> getter_result { return (*self.*getter)(); },
                [=](setter_arg&& val) { (*self.*setter)(std::move(val)); });
}

template<typename T, fundamental_type FT = type_to_ftype<T>::ftype>
value_property* as_value_property(T* ptr)
{
    return as_value_property<FT>([ptr]() -> const T& { return *ptr; },
                                 [ptr](T&& val) { *ptr = std::move(val); });
}

template<typename List,
         fundamental_type FT = type_to_ftype<typename List::value_type>::ftype>
list_property* as_list_property(List* ptr)
{
    return new list_property_impl<List, FT>(ptr);
}

template<typename Map,
         fundamental_type KT = type_to_ftype<typename Map::key_type>::ftype,
         fundamental_type VT = type_to_ftype<typename Map::mapped_type>::ftype>
map_property* as_map_property(Map* ptr)
{
    return new map_property_impl<Map, KT, VT>(ptr);
}

struct property_ptr
{

    enum flag_type { is_null, is_vp, is_lp, is_mp } m_flag;

    union
    {
        value_property* m_vp;
        list_property* m_lp;
        map_property* m_mp;
    };

    void set(value_property* vp)
    {
        m_flag = is_vp;
        m_vp = vp;
    }

    void set(list_property* lp)
    {
        m_flag = is_lp;
        m_lp = lp;
    }

    void set(map_property* mp)
    {
        m_flag = is_mp;
        m_mp = mp;
    }

    void destroy()
    {
        switch (m_flag)
        {
         case is_vp: delete m_vp; break;
         case is_lp: delete m_lp; break;
         case is_mp: delete m_mp; break;
         default: break;
        }
        m_flag = is_null;
    }

    void move_from(property_ptr& other)
    {
        m_flag = other.m_flag;
        switch (other.m_flag)
        {
         case is_vp: m_vp = other.m_vp; break;
         case is_lp: m_lp = other.m_lp; break;
         case is_mp: m_mp = other.m_mp; break;
         default: break;
        }
        other.m_flag = is_null;
    }

    value_property* get(std::true_type, std::false_type, std::false_type)
    {
        if (m_flag != is_vp) throw std::bad_cast();
        return m_vp;
    }

    list_property* get(std::false_type, std::true_type, std::false_type)
    {
        if (m_flag != is_lp) throw std::bad_cast();
        return m_lp;
    }

    map_property* get(std::false_type, std::false_type, std::true_type)
    {
        if (m_flag != is_mp) throw std::bad_cast();
        return m_mp;
    }

 public:

    property_ptr(value_property* ptr) { set(ptr); }

    property_ptr(list_property* ptr) { set(ptr); }

    property_ptr(map_property* ptr) { set(ptr); }

    property_ptr() : m_flag(is_null) { }

    property_ptr(property_ptr&& other)
    {
        move_from(other);
    }

    property_ptr& operator=(property_ptr&& other)
    {
        destroy();
        move_from(other);
        return *this;
    }

    ~property_ptr()
    {
        destroy();
    }

    property_ptr(const property_ptr&) = delete;

    property_ptr& operator=(const property_ptr&) = delete;

    inline bool is_value_property() const { return m_flag == is_vp; }

    inline bool is_list_property() const { return m_flag == is_lp; }

    inline bool is_map_property() const { return m_flag == is_mp; }

    inline explicit operator bool() const { return m_flag != is_null; }

    inline bool operator==(const std::nullptr_t&)
    {
        return m_flag == is_null;
    }

    inline bool operator!=(const std::nullptr_t&)
    {
        return m_flag != is_null;
    }

    template<typename T>
    T* as()
    {
        return get(std::is_same<T, value_property>(),
                   std::is_same<T, list_property>(),
                   std::is_same<T, map_property>());
    }

};

/**
 * @brief
 */
class abstract_object
{

    inline void push_back() { }

    template<typename T0, typename... Tn>
    void push_back(T0* ptr0, Tn*... ptrs)
    {
        static_assert(   std::is_same<T0, value_property>::value
                      || std::is_same<T0, list_property>::value
                      || std::is_same<T0, map_property>::value,
                      "invalid type");
        m_properties.push_back(property_ptr(ptr0));
        push_back(ptrs...);
    }

 public:

    typedef std::vector<property_ptr> pptr_vector;

    abstract_object(pptr_vector&& pvec) : m_properties(std::move(pvec)) { }

    template<typename T0, typename... Tn>
    abstract_object(T0* ptr0, Tn*... ptrs)
    {
        push_back(ptr0, ptrs...);
    }

    abstract_object() = default;
    abstract_object(abstract_object&&) = default;
    abstract_object& operator=(abstract_object&&) = default;

    abstract_object(const abstract_object&) = delete;
    abstract_object& operator=(const abstract_object&) = delete;

    size_t properties()
    {
        return m_properties.size();
    }

    property_ptr& property(size_t pos)
    {
        return m_properties[pos];
    }

 private:

    pptr_vector m_properties;

};

class sink
{

 public:

    virtual void write(abstract_object&) = 0;

};

struct xml_sink_helper
{
    std::ostringstream& ostr;
    const std::string& indent;
    xml_sink_helper(std::ostringstream& mostr, const std::string& idn) : ostr(mostr), indent(idn) { }
    template<typename T>
    void operator()(const T& what)
    {
        static const fundamental_type ftype = type_to_ftype<T>::ftype;
        ostr << indent
             << "<" << fundamental_type_name(ftype) << ">"
             << what
             << "</" << fundamental_type_name(ftype) << ">\n";
    }
    void operator()(const std::u16string&) { }
    void operator()(const std::u32string&) { }
};

class xml_sink
{

    static const char br = '\n';

    std::ostringstream ostr;

    void append(const std::string& indentation, const ft_value& what)
    {
        what.apply(xml_sink_helper(ostr, indentation));
    }

 public:

    void write(abstract_object& obj)
    {
        ostr << "<object>" << br;
        for (size_t i = 0; i < obj.properties(); ++i)
        {
            property_ptr& pptr = obj.property(i);
            if (pptr.is_value_property())
            {
                auto vp = pptr.as<value_property>();
                ft_value val(vp->type());
                vp->get(val);
                append("    ", val);
            }
        }
        ostr << "</object>";
    }

    std::string str() const { return ostr.str(); }

};

class source
{

 public:

    virtual void read(abstract_object&) = 0;

};

struct point_struct
{
    std::uint32_t x, y, z;
};

class point_class
{

    std::uint32_t m_x, m_y, m_z;

 public:

    point_class() : m_x(0), m_y(0), m_z(0) { }

    point_class(std::uint32_t mx, std::uint32_t my, std::uint32_t mz)
        : m_x(mx), m_y(my), m_z(mz)
    {
    }

    std::uint32_t x() const { return m_x; }

    std::uint32_t y() const { return m_y; }

    std::uint32_t z() const { return m_z; }

    void set_x(std::uint32_t value) { m_x = value; }

    void set_y(std::uint32_t value) { m_y = value; }

    void set_z(std::uint32_t value) { m_z = value; }

};

void plot(value_property* vp)
{
    ft_value v(vp->type());
    vp->get(v);
    switch (v.type())
    {
    case ft_uint32: cout << ft_value_cast<ft_uint32>(v) << " "; break;
    default: break;
    }
}

void plot(property_ptr& pptr)
{
    if (pptr.is_value_property())
    {
        plot(pptr.as<value_property>());
    }
}

void plot(abstract_object& what, const std::string& what_name)
{
    cout << what_name << " (" << what.properties() << " properties): ";
    for (size_t i = 0; i < what.properties(); ++i)
    {
        plot(what.property(i));
    }
    cout << endl;
}

std::size_t test__serialization()
{

    CPPA_TEST(test__serialization);

    {
        ft_value v1(42);
        ft_value v2(42);
        CPPA_CHECK_EQUAL(v1, v2);
        CPPA_CHECK_EQUAL(v1, 42);
        CPPA_CHECK_EQUAL(42, v2);
        CPPA_CHECK(v2 != static_cast<std::int8_t>(42));
    }

    auto manipulate_point = [&](abstract_object& pt) {
        CPPA_CHECK_EQUAL(pt.properties(), 3);
        if (pt.properties() == 3)
        {
            for (size_t i = 0; i < 3; ++i)
            {
                property_ptr& pptr = pt.property(i);
                bool is_value_property = pptr.is_value_property();
                CPPA_CHECK(is_value_property);
                if (is_value_property)
                {
                    CPPA_CHECK_EQUAL(pptr.as<value_property>()->type(),
                                     ft_uint32);
                    if (i == 1)
                    {
                        auto vptr = pptr.as<value_property>();
                        // value must be 2
                        ft_value val(ft_uint32);
                        vptr->get(val);
                        CPPA_CHECK_EQUAL(ft_value_cast<ft_uint32>(val), 2);
                        vptr->set(ft_value(static_cast<std::uint32_t>(22)));
                        vptr->get(val);
                        CPPA_CHECK_EQUAL(ft_value_cast<ft_uint32>(val), 22);
                    }
                }
            }
        }
    };

    // test as_value_property with direct member access
    {
        point_struct pt = { 1, 2, 3 };
        abstract_object abstract_pt { as_value_property(&pt.x),
                                    as_value_property(&pt.y),
                                    as_value_property(&pt.z) };
        manipulate_point(abstract_pt);
        CPPA_CHECK_EQUAL(pt.x, 1);
        CPPA_CHECK_EQUAL(pt.y, 22);
        CPPA_CHECK_EQUAL(pt.z, 3);
        xml_sink xs;
        xs.write(abstract_pt);
        cout << "XML:" << endl << xs.str() << endl;
    }

    // test as_value_property with direct getter / setter implementations
    {
        point_class pt = { 1, 2, 4 };
        abstract_object abstract_pt = {
            as_value_property<ft_uint32>(&pt, &point_class::x, &point_class::set_x),
            as_value_property<ft_uint32>(&pt, &point_class::y, &point_class::set_y),
            as_value_property<ft_uint32>(&pt, &point_class::z, &point_class::set_z)
        };
        manipulate_point(abstract_pt);
        CPPA_CHECK_EQUAL(pt.x(), 1);
        CPPA_CHECK_EQUAL(pt.y(), 22);
        CPPA_CHECK_EQUAL(pt.z(), 4);
    }

    {
        std::string str = "Hello World";
        std::unique_ptr<value_property> p(as_value_property(&str));
        p->set(std::string("foobar"));
        CPPA_CHECK_EQUAL(str, "foobar");
    }

    {
        std::vector<std::int32_t> ints;
        std::unique_ptr<list_property> p(as_list_property(&ints));
        p->push_back(static_cast<std::int32_t>(1));
        p->push_back(static_cast<std::int32_t>(2));
        p->push_back(static_cast<std::int32_t>(3));
        for (std::unique_ptr<list_property::iterator> i(p->begin()); !i->at_end(); i->next())
        {
            cout << ft_value_cast<ft_int32>(i->get()) << " ";
        }
        cout << endl;
        CPPA_CHECK_EQUAL(ints.size(), 3);
        CPPA_CHECK_EQUAL(ints, (std::vector<std::int32_t>({1, 2, 3})));
    }

    {
        std::map<std::int32_t, std::string> strings;
        std::unique_ptr<map_property> p(as_map_property(&strings));
        p->insert(static_cast<std::int32_t>(2), std::string("two"));
        p->insert(static_cast<std::int32_t>(1), std::string("one"));
        p->insert(static_cast<std::int32_t>(4), std::string("four"));
        std::map<std::int32_t, std::string> verification_map = {
            { 1, "one" },
            { 2, "two" },
            { 4, "four" }
        };
        CPPA_CHECK_EQUAL(strings.size(), 3);
        CPPA_CHECK_EQUAL(strings, verification_map);
        // also check equality by iterators
        if (strings.size() == verification_map.size())
        {
            auto siter = strings.begin();
            auto send = strings.end();
            std::unique_ptr<map_property::iterator> viter(p->begin());
            while (siter != send)
            {
                CPPA_CHECK_EQUAL(ft_value_cast<ft_int32>(viter->key()),
                                 siter->first);
                CPPA_CHECK_EQUAL(ft_value_cast<ft_u8string>(viter->value()),
                                 siter->second);
                viter->next();
                ++siter;
            }
        }
    }

    ft_value v1("Hello World");

    auto plot_objects = [] (const std::vector<cppa::object>& objs)
    {
        cout << "{ ";
        bool first = true;
        for (const cppa::object& o : objs)
        {
            if (first) first = false;
            else cout << ", ";
            cout << o.type().name() << "(";
            if (o.type() == typeid(std::string))
                cout << "\"" << o.to_string() << "\"";
            else
                cout << o.to_string();
            cout << ")";
        }
        cout << " }" << endl;
    };

    plot_objects({
        cppa::uniform_typeid<int>()->create(),
        cppa::uniform_typeid<std::string>()->create()
    });

    return CPPA_TEST_RESULT;

}
