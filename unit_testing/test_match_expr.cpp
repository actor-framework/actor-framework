#include "test.hpp"

#include "caf/all.hpp"
#include "caf/detail/safe_equal.hpp"

using std::cout;
using std::endl;

using namespace caf;

#define CAF_CHECK_VARIANT(Handler, Message, Type, Expected)                    \
  {                                                                            \
    auto msg = Message;                                                        \
    auto res = Handler(msg);                                                   \
    if (get<Type>(&res) == nullptr) {                                          \
      CAF_FAILURE("result has invalid type");                                  \
    } else if (!caf::detail::safe_equal(*get<Type>(&res), Expected)) {         \
      CAF_FAILURE("expected " << Expected << " found " << *get<Type>(&res));   \
    } else {                                                                   \
      CAF_CHECKPOINT();                                                        \
    }                                                                          \
  }                                                                            \
  static_cast<void>(0)

#define CAF_CHECK_VARIANT_UNIT(Handler, Message)                               \
  {                                                                            \
    auto msg = Message;                                                        \
    auto res = Handler(msg);                                                   \
    if (get<unit_t>(&res) == nullptr) {                                        \
      CAF_FAILURE("result has invalid type");                                  \
    } else {                                                                   \
      CAF_CHECKPOINT();                                                        \
    }                                                                          \
  }                                                                            \
  static_cast<void>(0)

#define CAF_CHECK_OPT_MSG(Handler, Message, Type, Expected)                    \
  {                                                                            \
    auto msg = Message;                                                        \
    auto res = Handler(msg);                                                   \
    if (!res) {                                                                \
      CAF_FAILURE("result is none");                                           \
    } else if (!res->match_element<Type>(0)) {                                 \
      CAF_FAILURE("result has invalid type: expected: "                        \
                  << typeid(Expected).name()                                   \
                  << ", found: " << typeid(Type).name());                      \
    } else if (!caf::detail::safe_equal(res->get_as<Type>(0), Expected)) {     \
      CAF_FAILURE("expected " << Expected << " found "                         \
                              << res->get_as<Type>(0));                        \
    } else {                                                                   \
      CAF_CHECKPOINT();                                                        \
    }                                                                          \
  }                                                                            \
  static_cast<void>(0)

#define CAF_CHECK_OPT_MSG_VOID(Call)                                           \
  {                                                                            \
    auto res = Call;                                                           \
    if (!res) {                                                                \
      CAF_FAILURE("result has invalid type: optional is none");                \
    } else if (!res->empty()) {                                                \
      CAF_FAILURE("result has invalid type: tuple is not empty");              \
    } else {                                                                   \
      CAF_CHECKPOINT();                                                        \
    }                                                                          \
  }                                                                            \
  static_cast<void>(0)

#define CAF_CHECK_OPT_MSG_NONE(Call)                                           \
  {                                                                            \
    auto res = Call;                                                           \
    if (res) {                                                                 \
      if (res->empty()) {                                                      \
        CAF_FAILURE("result has invalid type: expected none, "                 \
                    << "found an empty tuple");                                \
      } else {                                                                 \
        CAF_FAILURE("result has invalid type: expected none, "                 \
                    << "found a non-empty tuple ");                            \
      }                                                                        \
    } else {                                                                   \
      CAF_CHECKPOINT();                                                        \
    }                                                                          \
  }                                                                            \
  static_cast<void>(0)

struct printer {
  inline void operator()() const{
    cout << endl;
  }
  template<typename V, typename... Vs>
  void operator()(const optional<V>& v, const Vs&... vs) const {
    if (v) {
      cout << *v << " ";
    } else {
      cout << "[none] ";
    }
    (*this)(vs...);
  }
  template<typename V, typename... Vs>
  void operator()(const V& v, const Vs&... vs) const {
    cout << v << " ";
    (*this)(vs...);
  }

};

int main() {
  CAF_TEST(test_match_expr);

  auto g = guarded(std::equal_to<int>{}, 5);
  auto t0 = std::make_tuple(unit, g, unit);
  auto t1 = std::make_tuple(4, 5, 6);
  auto t2 = std::make_tuple(5, 6, 7);
  detail::lifted_fun_zipper zip;
  auto is = detail::get_indices(t2);
  auto r0 = detail::tuple_zip(zip, is, t0, t1);
  printer p;
  detail::apply_args(p, is, r0);

  { // --- types only ---
    // check on() usage
    auto m0 = on<int>() >> [](int) {};
    // auto m0r0 = m0(make_message(1));
    CAF_CHECK_VARIANT_UNIT(m0, make_message(1));
    // check lifted functor
    auto m1 = detail::lift_to_match_expr([](float) {});
    CAF_CHECK_VARIANT_UNIT(m1, make_message(1.f));
    // check _.or_else(_)
    auto m2 = m0.or_else(m1);
    CAF_CHECK_VARIANT_UNIT(m2, make_message(1));
    CAF_CHECK_VARIANT_UNIT(m2, make_message(1.f));
    // check use of match_expr_concat
    auto m3 = detail::match_expr_concat(
      m0, m1, detail::lift_to_match_expr([](double) {}));
    CAF_CHECK_OPT_MSG_VOID(m3->invoke(make_message(1)));
    CAF_CHECK_OPT_MSG_VOID(m3->invoke(make_message(1.f)));
    CAF_CHECK_OPT_MSG_VOID(m3->invoke(make_message(1.)));
    CAF_CHECK_OPT_MSG_NONE(m3->invoke(make_message("1")));
  }

  { // --- same with guards ---
    auto m0 = on(1) >> [](int i) { CAF_CHECK_EQUAL(i, 1); };
    CAF_CHECK_VARIANT_UNIT(m0, make_message(1));
    // check lifted functor
    auto m1 = on(1.f) >> [](float) {};
    CAF_CHECK_VARIANT_UNIT(m1, make_message(1.f));
    // check _.or_else(_)
    auto m2 = m0.or_else(m1);
    CAF_CHECK_VARIANT_UNIT(m2, make_message(1));
    CAF_CHECK_VARIANT_UNIT(m2, make_message(1.f));
    // check use of match_expr_concat
    auto m3 = detail::match_expr_concat(m0, m1, on(1.) >> [](double) {});
    CAF_CHECK_OPT_MSG_VOID(m3->invoke(make_message(1)));
    CAF_CHECK_OPT_MSG_NONE(m3->invoke(make_message(2)));
    CAF_CHECK_OPT_MSG_VOID(m3->invoke(make_message(1.f)));
    CAF_CHECK_OPT_MSG_NONE(m3->invoke(make_message(2.f)));
    CAF_CHECK_OPT_MSG_VOID(m3->invoke(make_message(1.)));
    CAF_CHECK_OPT_MSG_NONE(m3->invoke(make_message(2.)));
    CAF_CHECK_OPT_MSG_NONE(m3->invoke(make_message("1")));
  }

  { // --- mixin it up with message_handler
    // check on() usage
    message_handler m0{on<int>() >> [](int) {}};
    CAF_CHECK_OPT_MSG_VOID(m0(make_message(1)));
    // check lifted functor
    auto m1 = detail::lift_to_match_expr([](float) {});
    CAF_CHECK_VARIANT_UNIT(m1, make_message(1.f));
    // check _.or_else(_)
    auto m2 = m0.or_else(m1);
    CAF_CHECK_OPT_MSG_VOID(m2(make_message(1)));
    CAF_CHECK_OPT_MSG_VOID(m2(make_message(1.f)));
    // check use of match_expr_concat
    auto m3 = detail::match_expr_concat(
      m0, m1, detail::lift_to_match_expr([](double) {}));
    CAF_CHECK_OPT_MSG_VOID(m3->invoke(make_message(1)));
    CAF_CHECK_OPT_MSG_VOID(m3->invoke(make_message(1.)));
    CAF_CHECK_OPT_MSG_VOID(m3->invoke(make_message(1.f)));
  }

  { // --- use match_expr with result ---
    auto m4 = on<int>() >> [](int i) { return i; };
    CAF_CHECK_VARIANT(m4, make_message(42), int, 42);
    auto m5 = on<float>() >> [](float f) { return f; };
    CAF_CHECK_VARIANT(m5, make_message(4.2f), float, 4.2f);
    auto m6 = m4.or_else(m5);
    CAF_CHECK_VARIANT(m6, make_message(4.2f), float, 4.2f);
    CAF_CHECK_VARIANT(m6, make_message(42), int, 42);
  }

  { // --- storing some match_expr in a behavior ---
    behavior m5{on(1) >> [] { return 2; }, on(1.f) >> [] { return 2.f; },
          on(1.) >> [] { return 2.; }};
    CAF_CHECK_OPT_MSG(m5, make_message(1), int, 2);
    CAF_CHECK_OPT_MSG(m5, make_message(1.), double, 2.);
    CAF_CHECK_OPT_MSG(m5, make_message(1.f), float, 2.f);
  }

  return CAF_TEST_RESULT();
}
