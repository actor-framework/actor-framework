#include "caf/fwd.hpp"
#include "caf/test/bdd_dsl.hpp"
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
bool inspect(Inspector& f, dummy_struct& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
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
  friend bool inspect(Inspector& f, fail_on_copy& x) {
    return f.object(x).fields(f.field("value", x.value));
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
  friend bool inspect(Inspector& f, i32_wrapper& x) {
    return f.apply(x.value);
  }
};

struct i64_wrapper {
  // Initialized in meta_object.cpp.
  static size_t instances;

  int64_t value;

  i64_wrapper() : value(0) {
    ++instances;
  }

  explicit i64_wrapper(int64_t val) : value(val) {
    ++instances;
  }

  ~i64_wrapper() {
    --instances;
  }

  template <class Inspector>
  friend bool inspect(Inspector& f, i64_wrapper& x) {
    return f.apply(x.value);
  }
};

struct my_request {
  int32_t a = 0;
  int32_t b = 0;
  my_request() = default;
  my_request(int a, int b) : a(a), b(b) {
    // nop
  }
};

[[maybe_unused]] inline bool operator==(const my_request& x,
                                        const my_request& y) {
  return std::tie(x.a, x.b) == std::tie(y.a, y.b);
}

template <class Inspector>
bool inspect(Inspector& f, my_request& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}

struct raw_struct {
  std::string str;
};

template <class Inspector>
bool inspect(Inspector& f, raw_struct& x) {
  return f.object(x).fields(f.field("str", x.str));
}

[[maybe_unused]] inline bool operator==(const raw_struct& lhs,
                                        const raw_struct& rhs) {
  return lhs.str == rhs.str;
}

struct s1 {
  int value[3] = {10, 20, 30};
};

template <class Inspector>
bool inspect(Inspector& f, s1& x) {
  return f.apply(x.value);
}

struct s2 {
  int value[4][2] = {{1, 10}, {2, 20}, {3, 30}, {4, 40}};
};

template <class Inspector>
bool inspect(Inspector& f, s2& x) {
  return f.apply(x.value);
}

struct s3 {
  std::array<int, 4> value;
  s3() {
    std::iota(value.begin(), value.end(), 1);
  }
};

template <class Inspector>
bool inspect(Inspector& f, s3& x) {
  return f.apply(x.value);
}

struct test_array {
  int32_t value[4];
  int32_t value2[2][4];
};

template <class Inspector>
bool inspect(Inspector& f, test_array& x) {
  return f.object(x).fields(f.field("value", x.value),
                            f.field("value2", x.value2));
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
bool inspect(Inspector& f, test_empty_non_pod& x) {
  return f.object(x).fields();
}

enum class test_enum : int32_t {
  a,
  b,
  c,
};

// Implemented in serialization.cpp
std::string to_string(test_enum x);

template <class Inspector>
bool inspect(Inspector& f, test_enum& x) {
  auto get = [&x] { return static_cast<int32_t>(x); };
  auto set = [&x](int32_t val) {
    if (val >= 0 && val <= 2) {
      x = static_cast<test_enum>(val);
      return true;
    } else {
      return false;
    }
  };
  return f.apply(get, set);
}

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
bool inspect(Inspector& f, test_data& x) {
  return f.object(x).fields(f.field("i32", x.i32), f.field("i64", x.i64),
                            f.field("f32", x.f32), f.field("f64", x.f64),
                            f.field("ts", x.ts), f.field("te", x.te),
                            f.field("str", x.str));
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

template <class Inspector>
bool inspect(Inspector& f, dummy_enum_class& x) {
  auto get = [&x] { return static_cast<short>(x); };
  auto set = [&x](short val) {
    if (val >= 0 && val <= 1) {
      x = static_cast<dummy_enum_class>(val);
      return true;
    } else {
      return false;
    }
  };
  return f.apply(get, set);
}

enum class level : uint8_t { all, trace, debug, warning, error };

std::string to_string(level);

bool from_string(caf::string_view, level&);

bool from_integer(uint8_t, level&);

template <class Inspector>
bool inspect(Inspector& f, level& x) {
  return caf::default_enum_inspect(f, x);
}

enum dummy_enum { de_foo, de_bar };

template <class Inspector>
bool inspect(Inspector& f, dummy_enum& x) {
  using integer_type = std::underlying_type_t<dummy_enum>;
  auto get = [&x] { return static_cast<integer_type>(x); };
  auto set = [&x](integer_type val) {
    if (val <= 1) {
      x = static_cast<dummy_enum>(val);
      return true;
    } else {
      return false;
    }
  };
  return f.apply(get, set);
}

struct point {
  int32_t x;
  int32_t y;
};

[[maybe_unused]] constexpr bool operator==(point a, point b) noexcept {
  return a.x == b.x && a.y == b.y;
}

[[maybe_unused]] constexpr bool operator!=(point a, point b) noexcept {
  return !(a == b);
}

template <class Inspector>
bool inspect(Inspector& f, point& x) {
  return f.object(x).fields(f.field("x", x.x), f.field("y", x.y));
}

struct rectangle {
  point top_left;
  point bottom_right;
};

template <class Inspector>
bool inspect(Inspector& f, rectangle& x) {
  return f.object(x).fields(f.field("top-left", x.top_left),
                            f.field("bottom-right", x.bottom_right));
}

[[maybe_unused]] constexpr bool operator==(const rectangle& x,
                                           const rectangle& y) noexcept {
  return x.top_left == y.top_left && x.bottom_right == y.bottom_right;
}

[[maybe_unused]] constexpr bool operator!=(const rectangle& x,
                                           const rectangle& y) noexcept {
  return !(x == y);
}

struct circle {
  point center;
  int32_t radius;
};

template <class Inspector>
bool inspect(Inspector& f, circle& x) {
  return f.object(x).fields(f.field("center", x.center),
                            f.field("radius", x.radius));
}

[[maybe_unused]] constexpr bool operator==(const circle& x,
                                           const circle& y) noexcept {
  return x.center == y.center && x.radius == y.radius;
}

[[maybe_unused]] constexpr bool operator!=(const circle& x,
                                           const circle& y) noexcept {
  return !(x == y);
}

struct widget {
  std::string color;
  caf::variant<rectangle, circle> shape;
};

template <class Inspector>
bool inspect(Inspector& f, widget& x) {
  return f.object(x).fields(f.field("color", x.color),
                            f.field("shape", x.shape));
}

[[maybe_unused]] inline bool operator==(const widget& x,
                                        const widget& y) noexcept {
  return x.color == y.color && x.shape == y.shape;
}

[[maybe_unused]] inline bool operator!=(const widget& x,
                                        const widget& y) noexcept {
  return !(x == y);
}

struct dummy_user {
  std::string name;
  caf::optional<std::string> nickname;
};

template <class Inspector>
bool inspect(Inspector& f, dummy_user& x) {
  return f.object(x).fields(f.field("name", x.name),
                            f.field("nickname", x.nickname));
}

struct phone_book {
  std::string city;
  std::map<std::string, int64_t> entries;
};

[[maybe_unused]] constexpr bool operator==(const phone_book& x,
                                           const phone_book& y) noexcept {
  return std::tie(x.city, x.entries) == std::tie(y.city, y.entries);
}

[[maybe_unused]] constexpr bool operator!=(const phone_book& x,
                                           const phone_book& y) noexcept {
  return !(x == y);
}

template <class Inspector>
bool inspect(Inspector& f, phone_book& x) {
  return f.object(x).fields(f.field("city", x.city),
                            f.field("entries", x.entries));
}

// -- type IDs for for all unit test suites ------------------------------------

#define ADD_TYPE_ID(type) CAF_ADD_TYPE_ID(core_test, type)
#define ADD_ATOM(atom_name) CAF_ADD_ATOM(core_test, atom_name)

CAF_BEGIN_TYPE_ID_BLOCK(core_test, caf::first_custom_type_id)

  ADD_TYPE_ID((caf::stream<int32_t>) )
  ADD_TYPE_ID((caf::stream<std::pair<level, std::string>>) )
  ADD_TYPE_ID((caf::stream<std::string>) )
  ADD_TYPE_ID((circle))
  ADD_TYPE_ID((dummy_enum))
  ADD_TYPE_ID((dummy_enum_class))
  ADD_TYPE_ID((dummy_struct))
  ADD_TYPE_ID((dummy_tag_type))
  ADD_TYPE_ID((dummy_user))
  ADD_TYPE_ID((fail_on_copy))
  ADD_TYPE_ID((float_actor))
  ADD_TYPE_ID((foo_actor))
  ADD_TYPE_ID((i32_wrapper))
  ADD_TYPE_ID((i64_wrapper))
  ADD_TYPE_ID((int_actor))
  ADD_TYPE_ID((level))
  ADD_TYPE_ID((my_request))
  ADD_TYPE_ID((phone_book))
  ADD_TYPE_ID((point))
  ADD_TYPE_ID((raw_struct))
  ADD_TYPE_ID((rectangle))
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
  ADD_TYPE_ID((widget))

  ADD_ATOM(abc_atom)
  ADD_ATOM(get_state_atom)
  ADD_ATOM(name_atom)
  ADD_ATOM(sub0_atom)
  ADD_ATOM(sub1_atom)
  ADD_ATOM(sub2_atom)
  ADD_ATOM(sub3_atom)
  ADD_ATOM(sub4_atom)
  ADD_ATOM(hi_atom)
  ADD_ATOM(ho_atom)

CAF_END_TYPE_ID_BLOCK(core_test)

#undef ADD_TYPE_ID
#undef ADD_ATOM
