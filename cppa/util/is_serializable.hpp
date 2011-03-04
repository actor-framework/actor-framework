#ifndef IS_SERIALIZABLE_HPP
#define IS_SERIALIZABLE_HPP

namespace cppa {

struct serializer;
struct deserializer;

} // namespace cppa

namespace cppa { namespace util {

template<typename T>
class is_serializable
{

	template<typename A>
	static auto help_fun1(A* arg, serializer* s) -> decltype((*s) << (*arg))
	{
		return true;
	}

	template<typename A>
	static void help_fun1(A*, void*) { }

	typedef decltype(help_fun1((T*) 0, (serializer*) 0)) type1;

	template<typename A>
	static auto help_fun2(A* arg, deserializer* d) -> decltype((*d) >> (*arg))
	{
		return true;
	}

	template<typename A>
	static void help_fun2(A*, void*) { }

	typedef decltype(help_fun2((T*) 0, (deserializer*) 0)) type2;

 public:

	static const bool value =    std::is_same<  serializer&, type1>::value
							  && std::is_same<deserializer&, type2>::value;

};

} } // namespace cppa::util

#endif // IS_SERIALIZABLE_HPP
