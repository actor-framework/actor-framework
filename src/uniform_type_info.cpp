#include <map>
#include <set>
#include <string>
#include <atomic>
#include <limits>
#include <cstdint>
#include <sstream>
#include <type_traits>

#include "cppa/config.hpp"

#include "cppa/actor.hpp"
#include "cppa/any_type.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/default_object_base.hpp"

#include "cppa/detail/demangle.hpp"
#include "cppa/detail/to_uniform_name.hpp"
#include "cppa/detail/default_object_impl.hpp"

namespace std {

inline ostream& operator<<(ostream& o, const cppa::any_type&) { return o; }
inline istream& operator>>(istream& i, cppa::any_type&) { return i; }

inline ostream& operator<<(ostream& o, const cppa::actor_ptr&) { return o; }
inline istream& operator>>(istream& i, cppa::actor_ptr&) { return i; }

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

class wstring_obj : public cppa::util::default_object_base<std::wstring>
{

	typedef cppa::util::default_object_base<std::wstring> super;

 public:

	wstring_obj(const cppa::uniform_type_info* uti) : super(uti) { }

	void deserialize(cppa::deserializer&)
	{

	}

	void from_string(const std::string&)
	{

	}

	cppa::object* copy() const
	{
		return new wstring_obj(type());
	}

	std::string to_string() const
	{
		return "";
	}

	void serialize(cppa::serializer&) const
	{
	}

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
	push<Int>(ints, std::integral_constant<bool, std::numeric_limits<Int>::is_signed>());
}

template<typename Int0, typename Int1, typename... Ints>
void push(std::map<int, std::pair<string_set, string_set>>& ints)
{
	push<Int0>(ints, std::integral_constant<bool, std::numeric_limits<Int0>::is_signed>());
	push<Int1, Ints...>(ints);
}

} // namespace <anonymous>

namespace cppa { namespace detail { namespace {

class uniform_type_info_map
{

	typedef std::map<std::string, uniform_type_info*> uti_map;

	// maps typeid names to uniform type informations
	uti_map m_by_tname;

	// maps uniform names to uniform type informations
	uti_map m_by_uname;

	template<typename ObjImpl>
	void insert(const std::set<std::string>& tnames)
	{
		if (tnames.empty())
		{
			throw std::logic_error("tnames.empty()");
		}
		std::string uname = to_uniform_name(demangle(tnames.begin()->c_str()));
		auto uti = new default_uniform_type_info_impl<ObjImpl>(uname);
		for (const std::string& tname : tnames)
		{
			m_by_tname.insert(std::make_pair(tname, uti));
		}
		m_by_uname.insert(std::make_pair(uti->name(), uti));
	}

	template<typename T>
	void insert()
	{
		std::string tname(raw_name<T>());
		insert<default_object_impl<T>>({tname});
	}

 public:

	uniform_type_info_map()
	{
		insert<std::string>();
		insert<wstring_obj>({std::string(raw_name<std::wstring>())});
		insert<float>();
		if (sizeof(double) == sizeof(long double))
		{
			std::string dbl = raw_name<double>();
			std::string ldbl = raw_name<long double>();
			insert<default_object_impl<double>>({ dbl, ldbl });
		}
		else
		{
			insert<double>();
			insert<long double>();
		}
		insert<any_type>();
		insert<actor_ptr>();
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
		insert<default_object_impl<std::int8_t>>(ints[sizeof(std::int8_t)].first);
		insert<default_object_impl<std::uint8_t>>(ints[sizeof(std::uint8_t)].second);
		insert<default_object_impl<std::int16_t>>(ints[sizeof(std::int16_t)].first);
		insert<default_object_impl<std::uint16_t>>(ints[sizeof(std::uint16_t)].second);
		insert<default_object_impl<std::int32_t>>(ints[sizeof(std::int32_t)].first);
		insert<default_object_impl<std::uint32_t>>(ints[sizeof(std::uint32_t)].second);
		insert<default_object_impl<std::int64_t>>(ints[sizeof(std::int64_t)].first);
		insert<default_object_impl<std::uint64_t>>(ints[sizeof(std::uint64_t)].second);
		//insert<std::wstring>();
	}

	uniform_type_info* by_raw_name(const std::string& name)
	{
		auto i = m_by_tname.find(name);
		if (i != m_by_tname.end())
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
			return false;
		}
		m_by_uname.insert(std::make_pair(what->name(), what));
		for (const std::string& plain_name : plain_names)
		{
			if (!m_by_tname.insert(std::make_pair(plain_name, what)).second)
			{
				throw std::runtime_error(plain_name + " already mapped to an uniform_type_info");
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

} } } // namespace detail::<anonymous>


namespace {

std::atomic<int> s_ids;

inline int next_id() { return s_ids.fetch_add(1); }

} // namespace <anonymous>

namespace cppa {

uniform_type_info::uniform_type_info(const std::string& uniform_type_name)
		: m_id(next_id()), m_name(uniform_type_name)
{
}

uniform_type_info::~uniform_type_info()
{
}

uniform_type_info* uniform_type_info::by_type_info(const std::type_info& tinf)
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

bool uniform_type_info::announce(const std::type_info& plain_type,
								 uniform_type_info* uniform_type)
{
	string_set tmp = { std::string(raw_name(plain_type)) };
	if (!detail::s_uniform_type_info_map().insert(tmp, uniform_type))
	{
		delete uniform_type;
		return false;
	}
	return true;
}

std::vector<uniform_type_info*> uniform_type_info::get_all()
{
	return detail::s_uniform_type_info_map().get_all();
}

} // namespace cppa
