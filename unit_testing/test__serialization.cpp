#include <string>
#include <limits>
#include <vector>
#include <cstring>
#include <sstream>
#include <cstdint>
#include <typeinfo>
#include <iostream>
#include <type_traits>

#include "test.hpp"

#include "cppa/util.hpp"
#include "cppa/match.hpp"
#include "cppa/tuple.hpp"
#include "cppa/serializer.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/untyped_tuple.hpp"

#include "cppa/util.hpp"

using std::cout;
using std::cerr;
using std::endl;

using namespace cppa;
using namespace cppa::util;

class format_error : public std::exception
{

	std::string m_what;

 public:

	format_error(const std::string& what_str) : m_what(what_str) { }

	virtual const char* what() const throw()
	{
		return m_what.c_str();
	}

	virtual ~format_error() throw() { }

};

template<bool Streamable, typename T>
struct to_string_
{
	inline static std::string _(const T&) { return "-?-"; }
};

template<typename T>
struct to_string_<true, T>
{
	inline static std::string _(const T& what)
	{
		std::ostringstream oss;
		oss << what;
		return oss.str();
	}
};

template<typename T>
class has_to_string
{

	template<typename A>
	static auto cmp_help_fun(std::ostream& o, A* arg) -> decltype(o << *arg)
	{
		return true;
	}

	static void cmp_help_fun(std::ostream&, void*) { }

	typedef decltype(cmp_help_fun(*((std::ostream*) 0), (T*) 0)) result_type;

 public:

	static const bool value = std::is_same<std::ostream&, result_type>::value;

};

template<typename T>
struct meta_type
{
	static std::string to_string(const T& what)
	{
		return to_string_<has_to_string<T>::value, T>::_(what);
	}
	static T from_string(const std::string&)
	{
		throw format_error("");
	}
};

template<>
struct meta_type<std::string>
{
	static std::string to_string(const std::string& what)
	{
		std::string result;
		result.reserve(what.size() + 2);
		result  = "\"";
		result += what;
		result += "\"";
		return result;
	}
	static std::string from_string(const std::string& str)
	{
		return str;
	}
};

template<int N, typename Tuple>
struct mt_helper
{
	typedef typename util::type_at<N, Tuple>::type element_type;
	inline static void to_string(std::ostringstream& oss, const Tuple& t)
	{
		mt_helper<N - 1, Tuple>::to_string(oss, t);
		oss << meta_type<element_type>::to_string(t.get<N>());
		if (N < (Tuple::type_list_size - 1)) oss << ", ";
	}
};

template<typename Tuple>
struct mt_helper<0, Tuple>
{
	typedef typename util::type_at<0, Tuple>::type element_type;
	inline static void to_string(std::ostringstream& oss, const Tuple& t)
	{
		oss << meta_type<element_type>::to_string(t.get<0>());
		if (Tuple::type_list_size > 1) oss << ", ";
	}
};

template<typename... Types>
struct meta_type<tuple<Types...>>
{
	typedef tuple<Types...> tuple_type;
	static void serialize(const tuple_type& what, serializer& data_sink)
	{
	}
	static tuple_type deserialize(deserializer& data_source)
	{
		return "";
	}
	static std::string to_string(const tuple_type& what)
	{
		std::ostringstream oss;
		oss << "{ ";
		mt_helper<tuple_type::type_list_size - 1, tuple_type>::to_string(oss, what);
		oss << " }";
		return oss.str();
	}
	static tuple_type from_string(const std::string& str)
	{
		return str;
	}
};

template<typename... Types>
std::string to_string(const tuple<Types...>& what)
{
	return meta_type<tuple<Types...>>::to_string(what);
}

struct obj_types : util::abstract_type_list
{

	std::size_t m_size;
	const utype** m_arr;

 public:

	obj_types(const std::vector<intrusive_ptr<object>>& objs) : m_size(objs.size())
	{
		m_arr = new const utype*[m_size];
		for (std::size_t i = 0; i != m_size; ++i)
		{
			m_arr[i] = &(objs[i]->type());
		}
	}

	obj_types(const obj_types& other) : m_size(other.size())
	{
		m_arr = new const utype*[m_size];
		for (std::size_t i = 0; i != m_size; ++i)
		{
			m_arr[i] = other.m_arr[i];
		}
	}

	~obj_types()
	{
		delete m_arr;
	}

	virtual std::size_t size() const { return m_size; }

	virtual abstract_type_list* copy() const
	{
		return new obj_types(*this);
	}

	virtual const_iterator begin() const
	{
		return m_arr;
	}

	virtual const_iterator end() const
	{
		return m_arr + m_size;
	}

	virtual const utype& at(std::size_t pos) const
	{
		return *m_arr[pos];
	}

};

class obj_tuple : public detail::abstract_tuple
{

	obj_types m_types;
	std::vector<intrusive_ptr<object>> m_obj;

 public:

	obj_tuple(const std::vector<intrusive_ptr<object>>& objs) : m_types(objs)
	{
		for (std::size_t i = 0; i != objs.size(); ++i)
		{
			m_obj.push_back(objs[i]->copy());
		}
	}

	virtual void* mutable_at(std::size_t pos)
	{
		return m_obj[pos]->mutable_value();
	}

	virtual std::size_t size() const
	{
		return m_obj.size();
	}

	virtual abstract_tuple* copy() const
	{
		return new obj_tuple(m_obj);
	}

	virtual const void* at(std::size_t pos) const
	{
		return m_obj[pos]->value();
	}

	virtual const utype& utype_at(std::size_t pos) const
	{
		return m_obj[pos]->type();
	}

	virtual const util::abstract_type_list& types() const
	{
		return m_types;
	}

	virtual bool equal_to(const abstract_tuple&) const
	{
		return false;
	}

	virtual void serialize(serializer& s) const
	{
		s << static_cast<std::uint8_t>(m_obj.size());
		for (std::size_t i = 0; i < m_obj.size(); ++i)
		{
			decltype(m_obj[i]) o = m_obj[i];
			s << o->type().name();
			o->serialize(s);
		}
	}

};

struct io_buf : sink, source
{

	char* m_buf;
	std::size_t m_size;
	std::size_t m_rd_pos;
	std::size_t m_wr_pos;

	io_buf() : m_buf(new char[2048]), m_size(2048), m_rd_pos(0), m_wr_pos(0) { }

	~io_buf() { delete[] m_buf; }

	virtual std::size_t read_some(std::size_t buf_size, void* buf)
	{
		if ((m_rd_pos + buf_size) <= m_size)
		{
			memcpy(buf, m_buf + m_rd_pos, buf_size);
			m_rd_pos += buf_size;
			return buf_size;
		}
		else
		{
			auto left = m_size - m_rd_pos;
			return (left > 0) ? read_some(left, buf) : 0;
		}
	}

	virtual void read(std::size_t buf_size, void* buf)
	{
		if (read_some(buf_size, buf) < buf_size)
		{
			throw std::ios_base::failure("Not enough bytes available");
		}
	}

	virtual void write(std::size_t buf_size, const void* buf)
	{
		if (m_wr_pos + buf_size > m_size)
		{
			throw std::ios_base::failure("Buffer full");
		}
		memcpy(m_buf + m_wr_pos, buf, buf_size);
		m_wr_pos += buf_size;
	}

	virtual void flush()
	{
	}

};

template<std::size_t Pos, std::size_t Size, typename Tuple>
struct serialize_tuple_at
{
	inline static void _(serializer& s, const Tuple& t)
	{
		s << uniform_type_info<typename type_at<Pos, Tuple>::type>().name()
		  << t.get<Pos>();
		serialize_tuple_at<Pos + 1, Size, Tuple>::_(s, t);
	}
};

template<std::size_t Size, typename Tuple>
struct serialize_tuple_at<Size, Size, Tuple>
{
	inline static void _(serializer&, const Tuple&) { }
};

template<typename... Types>
serializer& operator<<(serializer& s, const tuple<Types...>& t)
{
	auto tsize = static_cast<std::uint8_t>(tuple<Types...>::type_list_size);
	s << tsize;
	serialize_tuple_at<0, tuple<Types...>::type_list_size, tuple<Types...>>::_(s, t);
	return s;
}

serializer& operator<<(serializer& s, const untyped_tuple& ut)
{
	/*
	auto tsize = static_cast<std::uint8_t>(ut.size());
	s << tsize;
	for (std::size_t i = 0; i < ut.size(); ++i)
	{
		s << ut.utype_at(i).name();

	}
	*/
	ut.vals()->serialize(s);
	return s;
}

deserializer& operator>>(deserializer& d, untyped_tuple& ut)
{
	std::uint8_t tsize;
	d >> tsize;
	std::vector<intrusive_ptr<object>> obj_vec;
	for (auto i = 0; i < tsize; ++i)
	{
		std::string type_name;
		d >> type_name;
		object* obj = uniform_type_info(type_name).create();
		obj->deserialize(d);
		obj_vec.push_back(obj);
	}
	cow_ptr<detail::abstract_tuple> vals(new obj_tuple(obj_vec));
	ut = untyped_tuple(vals);
	return d;
}

std::size_t test__serialization()
{

	CPPA_TEST(test__serialization);

	std::string str = "Hello World";

	CPPA_CHECK_EQUAL(to_string(make_tuple(str)),
					 "{ \"Hello World\" }");

	CPPA_CHECK_EQUAL(to_string(make_tuple(str, 42)),
					 "{ \"Hello World\", 42 }");

	char  v0 = 0x11;
	short v1 = 0x1122;
	int   v2 = 0x11223344;
	long  v3 = 0x1122334455667788;

	CPPA_CHECK_EQUAL(detail::swap_bytes(v0), 0x11);
	CPPA_CHECK_EQUAL(detail::swap_bytes(v1), 0x2211);
	CPPA_CHECK_EQUAL(detail::swap_bytes(v2), 0x44332211);
	CPPA_CHECK_EQUAL(detail::swap_bytes(v3), 0x8877665544332211);

	std::vector<intrusive_ptr<object>> obj_vec;

	obj_vec.reserve(2);

	obj_vec.push_back(uniform_type_info<std::string>().create());
	obj_vec.push_back(uniform_type_info<int>().create());

	cow_ptr<detail::abstract_tuple> vals(new obj_tuple(obj_vec));
	untyped_tuple ut0(vals);

	intrusive_ptr<io_buf> io0(new io_buf);

	auto t0 = make_tuple(42, "Hello World");

	{
		serializer s(io0);
		s << t0;
	}

	{
		deserializer d(io0);
		untyped_tuple ut1;
		d >> ut1;
		std::vector<std::size_t> mappings;
		bool does_match = match<int, any_type*, std::string>(ut1, mappings);
		CPPA_CHECK_EQUAL(does_match, true);
		if (does_match)
		{
			tuple_view<int, std::string> tv(ut1.vals(), std::move(mappings));
			CPPA_CHECK_EQUAL(tv.get<0>(), 42);
			CPPA_CHECK_EQUAL(tv.get<1>(), "Hello World");
		}
	}

	untyped_tuple ut2 = make_tuple("a", "b", 1, 2, 3);
	intrusive_ptr<io_buf> io1(new io_buf);

	{
		serializer s(io1);
		s << ut2;
	}

	{
		deserializer d(io1);
		untyped_tuple ut3;
		d >> ut3;
/*
		cout << "ut3 (size = " << ut3.size() << "): ";
		for (std::size_t i = 0; i < ut3.size(); ++i)
		{
			if (i > 0) cout << ", ";
			cout << ut3.utype_at(i).name();
		}
		cout << endl;
*/
		std::vector<std::size_t> mappings;
		bool does_match = match<std::string, std::string, int, int, int>(ut3, mappings);
		CPPA_CHECK_EQUAL(does_match, true);
		if (does_match)
		{
			tuple_view<std::string, std::string, int, int, int> tv(ut3.vals(), std::move(mappings));
			CPPA_CHECK_EQUAL(tv.get<0>(), "a");
			CPPA_CHECK_EQUAL(tv.get<1>(), "b");
			CPPA_CHECK_EQUAL(tv.get<2>(), 1);
			CPPA_CHECK_EQUAL(tv.get<3>(), 2);
			CPPA_CHECK_EQUAL(tv.get<4>(), 3);
		}
	}

	{
		std::vector<std::size_t> mappings;
		bool does_match = match<std::string, int>(ut0, mappings);
		CPPA_CHECK_EQUAL(does_match, true);
		if (does_match)
		{
			tuple_view<std::string, int> tv0(ut0.vals(), std::move(mappings));
			CPPA_CHECK_EQUAL(tv0.get<0>(), "");
			CPPA_CHECK_EQUAL(tv0.get<1>(), 0);
			tv0.get_ref<0>() = "Hello World";
			tv0.get_ref<1>() = 42;
			CPPA_CHECK_EQUAL(tv0.get<0>(), "Hello World");
			CPPA_CHECK_EQUAL(tv0.get<1>(), 42);
		}
	}

	auto t1 = make_tuple("a", "b", 1, 2, 3);
	auto t2 = make_tuple("a", "b", "c", 23.f, 1, 11, 2, 3);

	std::vector<intrusive_ptr<io_buf>> io_bufs = {
		new io_buf, new io_buf, new io_buf, new io_buf
	};

	{
		serializer s(io_bufs[0]);
		s << t1;
	}

	{
		serializer s1(io_bufs[1]);
		auto tmp1 = get_view<std::string, std::string, any_type*, int, any_type, int, int>(t2);
		s1 << tmp1;
		untyped_tuple tmp2 = tmp1;
		serializer s2(io_bufs[2]);
		s2 << tmp2;
	}

	{
		serializer s(io_bufs[3]);
		untyped_tuple tmp = t1;
		s << tmp;
	}

	for (auto i = 0; i < 4; ++i)
	{
		deserializer d(io_bufs[i]);
		untyped_tuple x;
		d >> x;
		std::vector<std::size_t> mappings;
		bool does_match = match<std::string, std::string, int, int, int>(x, mappings);
		CPPA_CHECK_EQUAL(does_match, true);
		if (does_match)
		{
			tuple_view<std::string, std::string, int, int, int> tv(x.vals(), std::move(mappings));
			CPPA_CHECK_EQUAL(tv.get<0>(), "a");
			CPPA_CHECK_EQUAL(tv.get<1>(), "b");
			CPPA_CHECK_EQUAL(tv.get<2>(), 1);
			CPPA_CHECK_EQUAL(tv.get<3>(), 2);
			CPPA_CHECK_EQUAL(tv.get<4>(), 3);
		}
	}

	return CPPA_TEST_RESULT;

}
