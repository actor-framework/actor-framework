#include <iostream>

template<class Out>
inline Out& print(Out& out) {
	return out;
}

template<class Out, typename A0, typename... Args>
inline Out& print(Out& out, const A0 arg0, const Args&... args) {
	return print(out << arg0, args...);
}

int main() {
	print(std::cout, "y", 'e', "s", '\n');
	return 0;
}

