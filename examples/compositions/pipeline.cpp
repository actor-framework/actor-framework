#include <set>
#include <tuple>
#include <string>
#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;

template <class T>
std::ostream& operator<<(std::ostream& out, const std::tuple<T>& x) {
  return out << std::get<0>(x) << endl;
}

using namespace caf;

namespace {

void print_iface(const actor_system::uncompressed_message_types_set& xs) {
  cout << "actor {" << endl;
  if (xs.empty())
    cout << "  any -> any";
  else
    for (auto& x : xs)
      cout << "  (" << join(x.first, ", ")
           << ") -> (" << join(x.second, ", ")
           << ")" << endl;
  cout << "}" << endl;
}

using plus_atom = atom_constant<atom("plus")>;
using minus_atom = atom_constant<atom("minus")>;
using divide_atom = atom_constant<atom("divide")>;
using multiply_atom = atom_constant<atom("multiply")>;

using calculator_actor =
  typed_actor<replies_to<plus_atom, double, double>::with<double>,
              replies_to<minus_atom, double, double>::with<double>,
              replies_to<divide_atom, double, double>::with<double>,
              replies_to<multiply_atom, double, double>::with<double>>;

calculator_actor::behavior_type calculator() {
  return {
    [](plus_atom, double x, double y) {
      return x + y;
    },
    [](minus_atom, double x, double y) {
      return x - y;
    },
    [](divide_atom, double x, double y) -> maybe<double> {
      if (y == 0.0)
        return none;
      return x / y;
    },
    [](multiply_atom, double x, double y) {
      return x * y;
    }
  };
}

using namespace caf::detail;

template <size_t I>
struct placeholder { };

template <class T, int X = std::is_placeholder<T>::value>
struct stl_placeholder_to_caf_placeholder_impl {
  using type = placeholder<X - 1>;
};

template <class T>
struct stl_placeholder_to_caf_placeholder_impl<T, 0> {
  using type = T;
};

template <class T>
struct stl_placeholder_to_caf_placeholder : stl_placeholder_to_caf_placeholder_impl<T> {
  // nop
};

template <class OriginalIns, class Ins, class Binds, size_t I, class... Ts>
struct single_binder_impl;

// type match in signature
template <class Os, class X, class... Xs, class... Ys, size_t I, class... Ts>
struct single_binder_impl<Os, type_list<X, Xs...>, type_list<X, Ys...>, I, Ts...>
    : single_binder_impl<Os, type_list<Xs...>, type_list<Ys...>, I + 1, Ts..., X>  {
  // nop
};

// hit wildcard in bind expression
template <class Os, class X, class... Xs, size_t Y, class... Ys, size_t I, class... Ts>
struct single_binder_impl<Os, type_list<X, Xs...>, type_list<placeholder<Y>, Ys...>, I, Ts...>
    : single_binder_impl<Os, type_list<Xs...>, type_list<Ys...>, I + 1, Ts..., typename tl_at<Os, Y>::type>  {
  // nop
};

// type mismatch between inputs and bind expression (bail out)
template <class Os, class X, class... Xs, class Y, class... Ys, size_t I, class... Ts>
struct single_binder_impl<Os, type_list<X, Xs...>, type_list<Y, Ys...>, I, Ts...> {
  using type = void;
};

// consumed whole bind expression
template <class OriginalIns, size_t I, class... Ts>
struct single_binder_impl<OriginalIns, type_list<>, type_list<>, I, Ts...> {
  using type = type_list<Ts...>;
};

template <class Inputs, class Outputs, class BindArgs,
          bool Mismatch = tl_size<Inputs>::value != tl_size<BindArgs>::value>
struct single_binder {
  using bound_inputs =
    typename single_binder_impl<
      Inputs,
      Inputs,
      typename tl_map<
        BindArgs,
        stl_placeholder_to_caf_placeholder
      >::type,
      0
    >::type;
  using type =
    typename std::conditional<
      std::is_same<void, bound_inputs>::value,
      void,
      typed_mpi<bound_inputs, Outputs>
    >::type;
};

template <class X, class Y, class Z>
struct single_binder<X, Y, Z, true> {
  using type = void;
};

template <class TypedMessagingInterfaceBindArgsPair>
struct signle_bind_caller;

template <class In, class Out, class BindArgs>
struct signle_bind_caller<type_pair<typed_mpi<In, Out>, BindArgs>>
  : single_binder<In, Out, BindArgs> {};

template <class Signatures, class... Ts>
struct binder;

template <class... Ss, class... Ts>
struct binder<type_list<Ss...>, Ts...> {
  using bind_args = type_list<Ts...>;
  using type =
    typename detail::tl_filter_not_type<
      typename tl_map<
        type_list<type_pair<Ss, bind_args>...>,
        signle_bind_caller
      >::type,
      void
    >::type;
};

template <class T, class... Ts>
int mybind(T x, Ts... xs) {
  typename binder<typename T::signatures, Ts...>::type* y = 0;
  cout << typeid(decltype(*y)).name() << endl;
  return 42;
}

} // namespace <anonymous>

int main(int argc, char** argv) {
  using namespace std::placeholders;
  actor_system system{argc, argv};
  auto calc = system.spawn(calculator);
  cout << "calc = ";
  print_iface(system.uncompressed_message_types(calc));
  auto multiplier = calc.bind(multiply_atom::value, _1, _2);
  auto f = make_function_view(multiplier);
  cout << "4 * 5 = " << f(4.0, 5.0) << endl;
  // tell functor to divide instead
  f.assign(calc.bind(divide_atom::value, _1, _2));
  cout << "4 / 5 = " << f(4.0, 5.0) << endl;
  // f(x) = x * x * x;
  mybind(calc, multiply_atom::value, _1, _1);
  mybind(calc, _3, _1, _2);
  /*
  auto g = make_function_adapter(doubler);
  cout << "7^2 = " << g(7.0) << endl;
  g.reset(doubler * doubler);
  cout << "7^3 = " << g(7.0) << endl;
  */
  anon_send_exit(calc, exit_reason::kill);
}
