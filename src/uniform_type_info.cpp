#include <map>
#include <set>
#include <locale>
#include <string>
#include <atomic>
#include <limits>
#include <cstdint>
#include <sstream>
#include <type_traits>

#include "cppa/config.hpp"

#include "cppa/actor.hpp"
#include "cppa/announce.hpp"
#include "cppa/any_type.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/void_type.hpp"

#include "cppa/detail/demangle.hpp"
#include "cppa/detail/to_uniform_name.hpp"
#include "cppa/detail/default_uniform_type_info_impl.hpp"

using cppa::util::void_type;

namespace std {

ostream& operator<<(ostream& o, const cppa::any_type&) { return o; }
istream& operator>>(istream& i, cppa::any_type&) { return i; }

ostream& operator<<(ostream& o, const cppa::actor_ptr&) { return o; }
istream& operator>>(istream& i, cppa::actor_ptr&) { return i; }

ostream& operator<<(ostream& o, const cppa::util::void_type&) { return o; }
istream& operator>>(istream& i, cppa::util::void_type&) { return i; }

} // namespace std

namespace {

inline const char* raw_name(const std::type_info& tinfo)
{
#ifdef CPPA_WINDOWS
    return tinfo.raw_name();
#else
    return tinfo.name();
#endif
}

template<typename T>
struct is_signed
        : std::integral_constant<bool, std::numeric_limits<T>::is_signed>
{
};

template<typename T>
inline const char* raw_name()
{
    return raw_name(typeid(T));
}

typedef std::set<std::string> string_set;

template<typename Int>
void push(std::map<int, std::pair<string_set, string_set>>& ints,
          std::integral_constant<bool, true>)
{
    ints[sizeof(Int)].first.insert(raw_name<Int>());
}

template<typename Int>
void push(std::map<int, std::pair<string_set, string_set>>& ints,
          std::integral_constant<bool, false>)
{
    ints[sizeof(Int)].second.insert(raw_name<Int>());
}

template<typename Int>
void push(std::map<int, std::pair<string_set, string_set>>& ints)
{
    push<Int>(ints, is_signed<Int>());
}

template<typename Int0, typename Int1, typename... Ints>
void push(std::map<int, std::pair<string_set, string_set>>& ints)
{
    push<Int0>(ints, is_signed<Int0>());
    push<Int1, Ints...>(ints);
}

} // namespace <anonymous>

namespace cppa { namespace detail { namespace {

class actor_ptr_type_info_impl : public util::uniform_type_info_base<actor_ptr>
{

 protected:

    void serialize(const void* ptr, serializer* sink) const
    {
        sink->begin_object(name());
        auto id = (*reinterpret_cast<const actor_ptr*>(ptr))->id();
        sink->write_value(id);
        sink->end_object();
    }

    void deserialize(void* ptr, deserializer* source) const
    {
        std::string cname = source->seek_object();
        if (cname != name())
        {
            throw std::logic_error("wrong type name found");
        }
        source->begin_object(cname);
        auto id = get<std::uint32_t>(source->read_value(pt_uint32));
        *reinterpret_cast<actor_ptr*>(ptr) = actor::by_id(id);
        source->end_object();
    }

};

class uniform_type_info_map
{

    typedef std::map<std::string, uniform_type_info*> uti_map;

    // maps raw typeid names to uniform type informations
    uti_map m_by_rname;

    // maps uniform names to uniform type informations
    uti_map m_by_uname;

    void insert(uniform_type_info* uti, const std::set<std::string>& tnames)
    {
        if (tnames.empty())
        {
            throw std::logic_error("tnames.empty()");
        }
        for (const std::string& tname : tnames)
        {
            m_by_rname.insert(std::make_pair(tname, uti));
        }
        m_by_uname.insert(std::make_pair(uti->name(), uti));
    }

    template<typename T>
    inline void insert(const std::set<std::string>& tnames)
    {
        insert(new default_uniform_type_info_impl<T>(), tnames);
    }

    template<typename T>
    inline void insert()
    {
        insert<T>({ std::string(raw_name<T>()) });
    }

 public:

    uniform_type_info_map()
    {
        insert<std::string>();
        insert<std::u16string>();
        insert<std::u32string>();
        insert(new actor_ptr_type_info_impl, { raw_name<actor_ptr>() });
        insert<float>();
        insert<cppa::util::void_type>();
        if (sizeof(double) == sizeof(long double))
        {
            std::string dbl = raw_name<double>();
            std::string ldbl = raw_name<long double>();
            insert<double>({ dbl, ldbl });
        }
        else
        {
            insert<double>();
            insert<long double>();
        }
        insert<any_type>();
        // first: signed
        // second: unsigned
        std::map<int, std::pair<string_set, string_set>> ints;
        push<char,
             signed char,
             unsigned char,
             short,
             signed short,
             unsigned short,
             short int,
             signed short int,
             unsigned short int,
             int,
             signed int,
             unsigned int,
             long int,
             signed long int,
             unsigned long int,
             long,
             signed long,
             unsigned long,
             long long,
             signed long long,
             unsigned long long,
             wchar_t,
             char16_t,
             char32_t>(ints);
        insert<std::int8_t>(ints[sizeof(std::int8_t)].first);
        insert<std::uint8_t>(ints[sizeof(std::uint8_t)].second);
        insert<std::int16_t>(ints[sizeof(std::int16_t)].first);
        insert<std::uint16_t>(ints[sizeof(std::uint16_t)].second);
        insert<std::int32_t>(ints[sizeof(std::int32_t)].first);
        insert<std::uint32_t>(ints[sizeof(std::uint32_t)].second);
        insert<std::int64_t>(ints[sizeof(std::int64_t)].first);
        insert<std::uint64_t>(ints[sizeof(std::uint64_t)].second);
    }

    uniform_type_info* by_raw_name(const std::string& name)
    {
        auto i = m_by_rname.find(name);
        if (i != m_by_rname.end())
        {
            return i->second;
        }
        return nullptr;
    }

    uniform_type_info* by_uniform_name(const std::string& name)
    {
        auto i = m_by_uname.find(name);
        if (i != m_by_uname.end())
        {
            return i->second;
        }
        return nullptr;
    }

    bool insert(std::set<std::string> plain_names,
                uniform_type_info* what)
    {
        if (m_by_uname.count(what->name()) > 0)
        {
            delete what;
            return false;
        }
        m_by_uname.insert(std::make_pair(what->name(), what));
        for (const std::string& plain_name : plain_names)
        {
            if (!m_by_rname.insert(std::make_pair(plain_name, what)).second)
            {
                std::string error_str = plain_name;
                error_str += " already mapped to an uniform_type_info";
                throw std::runtime_error(error_str);
            }
        }
        return true;
    }

    std::vector<uniform_type_info*> get_all()
    {
        std::vector<uniform_type_info*> result;
        result.reserve(m_by_uname.size());
        for (const uti_map::value_type& i : m_by_uname)
        {
            result.push_back(i.second);
        }
        return std::move(result);
    }

};

uniform_type_info_map& s_uniform_type_info_map()
{
    static uniform_type_info_map s_utimap;
    return s_utimap;
}

} } } // namespace cppa::detail::<anonymous>


namespace {

std::atomic<int> s_ids;

inline int next_id() { return s_ids.fetch_add(1); }

} // namespace <anonymous>

namespace cppa {

bool announce(const std::type_info& tinfo, uniform_type_info* utype)
{
    return detail::s_uniform_type_info_map().insert({ raw_name(tinfo) }, utype);
}

uniform_type_info::uniform_type_info(const std::string& uname)
    : m_id(next_id()), m_name(uname)
{
}

uniform_type_info::~uniform_type_info()
{
}

object uniform_type_info::create() const
{
    return { new_instance(), this };
}

const uniform_type_info*
uniform_type_info::by_type_info(const std::type_info& tinf)
{
    auto result = detail::s_uniform_type_info_map().by_raw_name(raw_name(tinf));
    if (!result)
    {
        throw std::runtime_error(std::string(raw_name(tinf))
                                 + " is an unknown typeid name");
    }
    return result;
}

uniform_type_info* uniform_type_info::by_uniform_name(const std::string& name)
{
    auto result = detail::s_uniform_type_info_map().by_uniform_name(name);
    if (!result)
    {
        throw std::runtime_error(name + " is an unknown typeid name");
    }
    return result;
}

object uniform_type_info::deserialize(deserializer* from) const
{
    auto ptr = new_instance();
    deserialize(ptr, from);
    return { ptr, this };
}

std::vector<uniform_type_info*> uniform_type_info::instances()
{
    return detail::s_uniform_type_info_map().get_all();
}

const uniform_type_info* uniform_typeid(const std::type_info& tinfo)
{
    return uniform_type_info::by_type_info(tinfo);
}

bool operator==(const uniform_type_info& lhs, const std::type_info& rhs)
{
    return lhs.equal(rhs);
}

} // namespace cppa
