#ifndef UNIFORM_TYPE_INFO_HPP
#define UNIFORM_TYPE_INFO_HPP

#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <typeinfo>

#include "cppa/object.hpp"
#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"

#include "cppa/detail/demangle.hpp"
#include "cppa/detail/to_uniform_name.hpp"

namespace cppa {

/**
 * @brief Provides a platform independent type name and (very primitive)
 *        reflection in combination with {@link cppa::object object}.
 *
 * The platform dependent type name (from GCC or Microsofts VC++ Compiler) is
 * translated to a (usually) shorter and platform independent name.
 *
 * This name is usually equal to the "in-sourcecode-name",
 * with a few exceptions:
 * - <i>std::string</i> is named <code>@str</code>
 * - <i>std::wstring</i> is named <code>@wstr</code>
 * - <i>integers</i> are named <code>@(i|u)$size</code>\n
 *   e.g.: @i32 is a 32 bit signed integer; @u16 is a 16 bit unsigned integer
 * - <i>anonymous namespace</i> is named <b>@_</b>\n
 *   e.g.: <code>namespace { class foo { }; }</code> is mapped to
 *   <code>@_::foo</code>
 */
class uniform_type_info : cppa::detail::comparable<uniform_type_info>
{

	template<typename T> friend uniform_type_info* uniform_typeid();

 public:

	class identifier : cppa::detail::comparable<identifier>
	{

		friend class uniform_type_info;
		friend class cppa::detail::comparable<identifier>;

		int m_value;

		identifier(int val) : m_value(val) { }

		// enable default copy and move constructors
		identifier(identifier&&) = default;
		identifier(const identifier&) = default;

		// disable any assignment
		identifier& operator=(identifier&&) = delete;
		identifier& operator=(const identifier&) = delete;

	 public:

		inline int compare(const identifier& other) const
		{
			return m_value - other.m_value;
		}

	};

 private:

	// unique identifier
	identifier m_id;

	// uniform type name
	std::string m_name;

	// prohibit all default constructors
	uniform_type_info() = delete;
	uniform_type_info(uniform_type_info&&) = delete;
	uniform_type_info(const uniform_type_info&) = delete;

	// prohibit all assignment operators
	uniform_type_info& operator=(uniform_type_info&&) = delete;
	uniform_type_info& operator=(const uniform_type_info&) = delete;

	static uniform_type_info* by_type_info(const std::type_info& tinfo);

 protected:

	uniform_type_info(const std::string& uniform_type_name);

 public:

	virtual ~uniform_type_info();

	inline const std::string& name() const { return m_name; }

	inline const identifier& id() const { return m_id; }

	inline int compare(const uniform_type_info& other) const
	{
		return id().compare(other.id());
	}

	virtual object* create() const = 0;

	static uniform_type_info* by_uniform_name(const std::string& uniform_name);

	static bool announce(const std::type_info& plain_type,
						 uniform_type_info* uniform_type);

	template<class ObjectImpl>
	static bool announce(const std::type_info& plain_type);

	static std::vector<uniform_type_info*> get_all();

};

template<typename T>
uniform_type_info* uniform_typeid()
{
	return uniform_type_info::by_type_info(typeid(T));
}

namespace detail {

template<class ObjectImpl>
class default_uniform_type_info_impl : public uniform_type_info
{

 public:

	default_uniform_type_info_impl(const std::string& tname)
		: uniform_type_info(tname)
	{
	}

	object* create /*[[override]]*/ () const
	{
		return new ObjectImpl(this);
	}

};

} // namespace detail

template<class ObjImpl>
bool uniform_type_info::announce(const std::type_info& plain)
{
	std::string uname = detail::to_uniform_name(detail::demangle(plain.name()));
	return announce(plain,
					new detail::default_uniform_type_info_impl<ObjImpl>(uname));
}

} // namespace cppa

#define CPPA_CONCAT_II(res) res
#define CPPA_CONCAT_I(lhs, rhs) CPPA_CONCAT_II(lhs ## rhs)
#define CPPA_CONCAT(lhs, rhs) CPPA_CONCAT_I( lhs, rhs )

/*
#ifdef __GNUC__
#	define CPPA_ANNOUNCE(what)                                                 \
	static const std::uint8_t CPPA_CONCAT( __unused_val , __LINE__ )           \
			__attribute__ ((unused))                                           \
			= cppa::detail::utype_impl< what >::announce_helper()
#else
#	define CPPA_ANNOUNCE(what)                                                 \
	static const std::uint8_t CPPA_CONCAT( __unused_val , __LINE__ )           \
			= cppa::detail::utype_impl< what >::announce_helper()
#endif
*/

#endif // UNIFORM_TYPE_INFO_HPP
