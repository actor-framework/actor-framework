#ifndef UNIFORM_TYPE_INFO_HPP
#define UNIFORM_TYPE_INFO_HPP

#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <typeinfo>
#include <type_traits>

#include "cppa/object.hpp"
#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"

#include "cppa/util/comparable.hpp"
#include "cppa/util/disjunction.hpp"
#include "cppa/util/callable_trait.hpp"

#include "cppa/detail/demangle.hpp"
#include "cppa/detail/to_uniform_name.hpp"

namespace cppa {

class uniform_type_info;

uniform_type_info* uniform_typeid(const std::type_info& tinfo);

/**
 * @brief Provides a platform independent type name and (very primitive)
 *        reflection in combination with {@link cppa::object object}.
 *
 * The platform dependent type name (from GCC or Microsofts VC++ Compiler) is
 * translated to a (usually) shorter and platform independent name.
 *
 * This name is usually equal to the "in-sourcecode-name",
 * with a few exceptions:
 * - @c std::string is named @c \@str
 * - @c std::wstring is named @c \@wstr
 * - @c integers are named <tt>\@(i|u)$size</tt>\n
 *   e.g.: @c \@i32 is a 32 bit signed integer; @c \@u16
 *   is a 16 bit unsigned integer
 * - the <em>anonymous namespace</em> is named @c \@_\n
 *   e.g.: @code namespace { class foo { }; } @endcode is mapped to
 *   @c \@_::foo
 * - {@link cppa::util::void_type} is named @c \@0
 */
class uniform_type_info : cppa::util::comparable<uniform_type_info>
{

	friend class object;

	// needs access to by_type_info()
	template<typename T>
	friend uniform_type_info* uniform_typeid();

	friend uniform_type_info* uniform_typeid(const std::type_info&);

 public:

	class identifier : cppa::util::comparable<identifier>
	{

		friend class uniform_type_info;

		int m_value;

		identifier(int val) : m_value(val) { }

		// enable copy and move constructors
		identifier(identifier&&) = default;
		identifier(const identifier&) = default;

		// disable assignment operators
		identifier& operator=(identifier&&) = delete;
		identifier& operator=(const identifier&) = delete;

	 public:

		// needed by cppa::detail::comparable<identifier>
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

	// disable copy and move constructors
	uniform_type_info(uniform_type_info&&) = delete;
	uniform_type_info(const uniform_type_info&) = delete;

	// disable assignment operators
	uniform_type_info& operator=(uniform_type_info&&) = delete;
	uniform_type_info& operator=(const uniform_type_info&) = delete;

	static uniform_type_info* by_type_info(const std::type_info& tinfo);

 protected:

	explicit uniform_type_info(const std::type_info& tinfo);

 public:

	/**
	 * @brief Get instances by its uniform name.
	 * @param uniform_name The libCPPA internal name for a type.
	 * @return The instance associated to @p uniform_name.
	 */
	static uniform_type_info* by_uniform_name(const std::string& uniform_name);

	/**
	 * @brief Get all instances.
	 * @return A vector with all known (announced) instances.
	 */
	static std::vector<uniform_type_info*> instances();

	virtual ~uniform_type_info();

	/**
	 * @brief Get the internal libCPPA name for this type.
	 * @return A string describing the libCPPA internal type name.
	 */
	inline const std::string& name() const { return m_name; }

	/**
	 * @brief Get the unique identifier of this instance.
	 * @return The unique identifier of this instance.
	 */
	inline const identifier& id() const { return m_id; }

	// needed by cppa::detail::comparable<uniform_type_info>
	inline int compare(const uniform_type_info& other) const
	{
		return id().compare(other.id());
	}

	/**
	 * @brief Add a new type mapping to the libCPPA internal type system.
	 * @return <code>true</code> if @p uniform_type was added as known
	 *         instance (mapped to @p plain_type); otherwise <code>false</code>
	 *         is returned and @p uniform_type was deleted.
	 */
	static bool announce(const std::type_info& plain_type,
						 uniform_type_info* uniform_type);

	/**
	 * auto concept value_type<typename T>
	 * {
	 *     T();
	 *     T(const T&);
	 *     bool operator==(const T&, const T&);
	 * }
	 */
	template<typename T,
			 class SerializeFun, class DeserializeFun,
			 class ToStringFun, class FromStringFun>
	static bool announce(const SerializeFun& sf, const DeserializeFun& df,
						 const ToStringFun& ts, const FromStringFun& fs);

	/**
	 * @brief Create an object of this type.
	 */
	virtual object create() const = 0;

	virtual object from_string(const std::string& str) const = 0;

 protected:

	// needed to implement subclasses
	inline void*& value_of(object& obj) const { return obj.m_value; }
	inline const void* value_of(const object& obj) const { return obj.m_value; }

	// object creation
	virtual object copy(const object& what) const = 0;

	// object modification
	virtual void destroy(object& what) const = 0;
	virtual void deserialize(deserializer& d, object& what) const = 0;

	// object inspection
	virtual std::string to_string(const object& obj) const = 0;
	virtual bool equal(const object& lhs, const object& rhs) const = 0;
	virtual void serialize(serializer& s, const object& what) const = 0;

};

template<typename T>
uniform_type_info* uniform_typeid()
{
	return uniform_type_info::by_type_info(typeid(T));
}

template<typename T,
		 class SerializeFun, class DeserializeFun,
		 class ToStringFun, class FromStringFun>
bool uniform_type_info::announce(const SerializeFun& sf,
								 const DeserializeFun& df,
								 const ToStringFun& ts,
								 const FromStringFun& fs)
{
	// check signature of SerializeFun::operator()
	typedef
		typename
		util::callable_trait<decltype(&SerializeFun::operator())>::arg_types
		sf_args;
	// assert arg_types == { serializer&, const T& } || { serializer&, T }
	static_assert(
		util::disjunction<
			std::is_same<sf_args, util::type_list<serializer&, const T&>>,
			std::is_same<sf_args, util::type_list<serializer&, T>>
		>::value,
		"Invalid signature of &SerializeFun::operator()");

	// check signature of DeserializeFun::operator()
	typedef
		typename
		util::callable_trait<decltype(&DeserializeFun::operator())>::arg_types
		df_args;
	// assert arg_types == { deserializer&, T& }
	static_assert(
		std::is_same<df_args, util::type_list<deserializer&, T&>>::value,
		"Invalid signature of &DeserializeFun::operator()");

	// check signature of ToStringFun::operator()
	typedef util::callable_trait<decltype(&ToStringFun::operator())> ts_sig;
	typedef typename ts_sig::arg_types ts_args;
	// assert result_type == std::string
	static_assert(
		std::is_same<std::string, typename ts_sig::result_type>::value,
		"ToStringFun::operator() doesn't return a string");
	// assert arg_types == { const T& } || { T }
	static_assert(
		util::disjunction<
			std::is_same<ts_args, util::type_list<const T&>>,
			std::is_same<ts_args, util::type_list<T>>
		>::value,
		"Invalid signature of &ToStringFun::operator()");

	// check signature of ToStringFun::operator()
	typedef util::callable_trait<decltype(&FromStringFun::operator())> fs_sig;
	typedef typename fs_sig::arg_types fs_args;
	// assert result_type == T*
	static_assert(
		std::is_same<T*, typename fs_sig::result_type>::value,
		"FromStringFun::operator() doesn't return T*");
	// assert arg_types == { const std::string& } || { std::string }
	static_assert(
		util::disjunction<
			std::is_same<fs_args, util::type_list<const std::string&>>,
			std::is_same<fs_args, util::type_list<std::string>>
		>::value,
		"Invalid signature of &FromStringFun::operator()");

	// "on-the-fly" implementation of uniform_type_info
	class utimpl : public uniform_type_info
	{

		SerializeFun m_serialize;
		DeserializeFun m_deserialize;
		ToStringFun m_to_string;
		FromStringFun m_from_string;

		inline T& to_ref(object& what) const
		{
			return *reinterpret_cast<T*>(this->value_of(what));
		}

		inline const T& to_ref(const object& what) const
		{
			return *reinterpret_cast<const T*>(this->value_of(what));
		}

	 protected:

		object copy(const object& what) const
		{
			return { new T(this->to_ref(what)), this };
		}

		object from_string(const std::string& str) const
		{
			return { m_from_string(str), this };
		}

		void destroy(object& what) const
		{
			delete reinterpret_cast<T*>(this->value_of(what));
			this->value_of(what) = nullptr;
		}

		void deserialize(deserializer& d, object& what) const
		{
			m_deserialize(d, to_ref(what));
		}

		std::string to_string(const object& obj) const
		{
			return m_to_string(to_ref(obj));
		}

		bool equal(const object& lhs, const object& rhs) const
		{
			return (lhs.type() == *this && rhs.type() == *this)
				   ? to_ref(lhs) == to_ref(rhs)
				   : false;
		}

		void serialize(serializer& s, const object& what) const
		{
			m_serialize(s, to_ref(what));
		}

	 public:

		utimpl(const SerializeFun& sfun, const DeserializeFun dfun,
			   const ToStringFun tfun, const FromStringFun ffun)
			: uniform_type_info(typeid(T))
			, m_serialize(sfun), m_deserialize(dfun)
			, m_to_string(tfun), m_from_string(ffun)
		{
		}

		object create() const
		{
			return { new T, this };
		}

	};

	return announce(typeid(T), new utimpl(sf, df, ts, fs));
}

bool operator==(const uniform_type_info& lhs, const std::type_info& rhs);

inline bool operator!=(const uniform_type_info& lhs, const std::type_info& rhs)
{
	return !(lhs == rhs);
}

inline bool operator==(const std::type_info& lhs, const uniform_type_info& rhs)
{
	return rhs == lhs;
}

inline bool operator!=(const std::type_info& lhs, const uniform_type_info& rhs)
{
	return !(rhs == lhs);
}

} // namespace cppa

#endif // UNIFORM_TYPE_INFO_HPP
