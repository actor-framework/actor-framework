#include "caf/fwd.hpp"
#include "caf/test/dsl.hpp"
#include "caf/type_id.hpp"
#include "caf/typed_actor.hpp"

#include <cstdint>
#include <numeric>
#include <string>
#include <utility>

// -- forward declarations for all unit test suites ----------------------------

using float_actor = caf::typed_actor<caf::reacts_to<float>>;

using int_actor = caf::typed_actor<caf::replies_to<int32_t>::with<int32_t>>;

using foo_actor
  = caf::typed_actor<caf::replies_to<int32_t, int32_t, int32_t>::with<int32_t>,
                     caf::replies_to<double>::with<double, double>>;

// A simple POD type.
struct dummy_struct {
  int a;
  std::string b;
};

[[maybe_unused]] inline bool operator==(const dummy_struct& x,
                                        const dummy_struct& y) {
  return x.a == y.a && x.b == y.b;
}

template <class Inspector>
decltype(auto) inspect(Inspector& f, dummy_struct& x) {
  return f(caf::meta::type_name("dummy_struct"), x.a, x.b);
}

// An empty type.
struct dummy_tag_type {};

constexpr bool operator==(dummy_tag_type, dummy_tag_type) {
  return true;
}

// Fails the test when copied. Implemented in message_lifetime.cpp.
struct fail_on_copy {
  int value;

  fail_on_copy() : value(0) {
    // nop
  }

  explicit fail_on_copy(int x) : value(x) {
    // nop
  }

  fail_on_copy(fail_on_copy&&) = default;

  fail_on_copy& operator=(fail_on_copy&&) = default;

  fail_on_copy(const fail_on_copy&);

  fail_on_copy& operator=(const fail_on_copy&);

  template <class Inspector>
  friend decltype(auto) inspect(Inspector& f, fail_on_copy& x) {
    return f(x.value);
  }
};

struct i32_wrapper {
  // Initialized in meta_object.cpp.
  static size_t instances;

  int32_t value;

  i32_wrapper() : value(0) {
    ++instances;
  }

  ~i32_wrapper() {
    --instances;
  }

  template <class Inspector>
  friend decltype(auto) inspect(Inspector& f, i32_wrapper& x) {
    return f(x.value);
  }
};

struct i64_wrapper {
  // Initialized in meta_object.cpp.
  static size_t instances;

  int64_t value;

  i64_wrapper() : value(0) {
    ++instances;
  }

  ~i64_wrapper() {
    --instances;
  }

  template <class Inspector>
  friend decltype(auto) inspect(Inspector& f, i64_wrapper& x) {
    return f(x.value);
  }
};

struct my_request {
  int32_t a;
  int32_t b;
};

[[maybe_unused]] inline bool operator==(const my_request& x,
                                        const my_request& y) {
  return std::tie(x.a, x.b) == std::tie(y.a, y.b);
}

template <class Inspector>
decltype(auto) inspect(Inspector& f, my_request& x) {
  return f(x.a, x.b);
}

struct raw_struct {
  std::string str;
};

template <class Inspector>
decltype(auto) inspect(Inspector& f, raw_struct& x) {
  return f(x.str);
}

[[maybe_unused]] inline bool operator==(const raw_struct& lhs,
                                        const raw_struct& rhs) {
  return lhs.str == rhs.str;
}

struct s1 {
  int value[3] = {10, 20, 30};
};

template <class Inspector>
decltype(auto) inspect(Inspector& f, s1& x) {
  return f(x.value);
}

struct s2 {
  int value[4][2] = {{1, 10}, {2, 20}, {3, 30}, {4, 40}};
};

template <class Inspector>
decltype(auto) inspect(Inspector& f, s2& x) {
  return f(x.value);
}

struct s3 {
  std::array<int, 4> value;
  s3() {
    std::iota(value.begin(), value.end(), 1);
  }
};

template <class Inspector>
decltype(auto) inspect(Inspector& f, s3& x) {
  return f(x.value);
}

struct test_array {
  int32_t value[4];
  int32_t value2[2][4];
};

template <class Inspector>
decltype(auto) inspect(Inspector& f, test_array& x) {
  return f(x.value, x.value2);
}

// Implemented in serialization.cpp.
struct test_empty_non_pod {
  test_empty_non_pod() = default;
  test_empty_non_pod(const test_empty_non_pod&) = default;
  test_empty_non_pod& operator=(const test_empty_non_pod&) = default;
  virtual void foo();
  virtual ~test_empty_non_pod();
};

template <class Inspector>
decltype(auto) inspect(Inspector& f, test_empty_non_pod&) {
  return f();
}

enum class test_enum : int32_t {
  a,
  b,
  c,
};

// Implemented in serialization.cpp
std::string to_string(test_enum x);

// Used in serializer.cpp and deserializer.cpp
struct test_data {
  int32_t i32;
  int64_t i64;
  float f32;
  double f64;
  caf::timestamp ts;
  test_enum te;
  std::string str;
};

template <class Inspector>
decltype(auto) inspect(Inspector& f, test_data& x) {
  return f(caf::meta::type_name("test_data"), x.i32, x.i64, x.f32, x.f64, x.ts,
           x.te, x.str);
}

[[maybe_unused]] inline bool operator==(const test_data& x,
                                        const test_data& y) {
  return std::tie(x.i32, x.i64, x.f32, x.f64, x.ts, x.te, x.str)
         == std::tie(y.i32, y.i64, y.f32, y.f64, y.ts, y.te, y.str);
}

enum class dummy_enum_class : short { foo, bar };

[[maybe_unused]] inline std::string to_string(dummy_enum_class x) {
  return x == dummy_enum_class::foo ? "foo" : "bar";
}

enum class level { all, trace, debug, warning, error };

enum dummy_enum { de_foo, de_bar };

// -- type IDs for for all unit test suites ------------------------------------

#define ADD_TYPE_ID(type) CAF_ADD_TYPE_ID(core_test, type)
#define ADD_ATOM(atom_name) CAF_ADD_ATOM(core_test, atom_name)

CAF_BEGIN_TYPE_ID_BLOCK(core_test, caf::first_custom_type_id)

  ADD_TYPE_ID((caf::stream<int32_t>) )
  ADD_TYPE_ID((caf::stream<std::string>) )
  ADD_TYPE_ID((caf::stream<std::pair<level, std::string>>) )
  ADD_TYPE_ID((dummy_enum))
  ADD_TYPE_ID((dummy_enum_class))
  ADD_TYPE_ID((dummy_struct))
  ADD_TYPE_ID((dummy_tag_type))
  ADD_TYPE_ID((fail_on_copy))
  ADD_TYPE_ID((float_actor))
  ADD_TYPE_ID((foo_actor))
  ADD_TYPE_ID((i32_wrapper))
  ADD_TYPE_ID((i64_wrapper))
  ADD_TYPE_ID((int_actor))
  ADD_TYPE_ID((level))
  ADD_TYPE_ID((my_request))
  ADD_TYPE_ID((raw_struct))
  ADD_TYPE_ID((s1))
  ADD_TYPE_ID((s2))
  ADD_TYPE_ID((s3))
  ADD_TYPE_ID((std::map<int32_t, int32_t>) )
  ADD_TYPE_ID((std::map<std::string, std::u16string>) )
  ADD_TYPE_ID((std::pair<level, std::string>) )
  ADD_TYPE_ID((std::tuple<int32_t, int32_t, int32_t>) )
  ADD_TYPE_ID((std::tuple<std::string, int32_t, uint32_t>) )
  ADD_TYPE_ID((std::vector<bool>) )
  ADD_TYPE_ID((std::vector<int32_t>) )
  ADD_TYPE_ID((std::vector<std::pair<level, std::string>>) )
  ADD_TYPE_ID((std::vector<std::string>) )
  ADD_TYPE_ID((test_array))
  ADD_TYPE_ID((test_empty_non_pod))
  ADD_TYPE_ID((test_enum))

  ADD_ATOM(abc_atom)
  ADD_ATOM(get_state_atom)
  ADD_ATOM(name_atom)

CAF_END_TYPE_ID_BLOCK(core_test)

#undef ADD_TYPE_ID
#undef ADD_ATOM
