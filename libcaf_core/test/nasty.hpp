#pragma once

#include "caf/inspector_access.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>

enum class weekday : uint8_t {
  monday,
  tuesday,
  wednesday,
  thursday,
  friday,
  saturday,
  sunday,
};

std::string to_string(weekday x);

bool parse(std::string_view input, weekday& dest);

template <class Inspector>
bool inspect(Inspector& f, weekday& x) {
  if (f.has_human_readable_format()) {
    auto get = [&x] { return to_string(x); };
    auto set = [&x](std::string str) { return parse(str, x); };
    return f.apply(get, set);
  } else {
    auto get = [&x] { return static_cast<uint8_t>(x); };
    auto set = [&x](uint8_t val) {
      if (val < 7) {
        x = static_cast<weekday>(val);
        return true;
      } else {
        return false;
      }
    };
    return f.apply(get, set);
  }
}

#define ADD_GET_SET_FIELD(type, name)                                          \
private:                                                                       \
  type name##_ = type{};                                                       \
                                                                               \
public:                                                                        \
  const auto& name() const noexcept {                                          \
    return name##_;                                                            \
  }                                                                            \
  void name(type value) {                                                      \
    name##_ = std::move(value);                                                \
  }

// A mean data type designed for maximum coverage of the inspect API.
class nasty {
public:
  static inline std::string_view tname = "nasty";

  using optional_type = std::optional<int32_t>;

  using variant_type = std::variant<std::string, int32_t>;

  using tuple_type = std::tuple<std::string, int32_t>;

  using optional_variant_type = std::optional<variant_type>;

  using optional_tuple_type = std::optional<tuple_type>;

  // Plain, direct access.
  int32_t field_01 = 0;

  // Plain, direct access, fallback (0).
  int32_t field_02 = 0;

  // Plain, direct access, invariant (>= 0).
  int32_t field_03 = 0;

  // Plain, direct access, fallback (0), invariant (>= 0).
  int32_t field_04 = 0;

  // Optional, direct access.
  optional_type field_05;

  // TODO: optional with fallback semantically useful?
  //
  // // Optional, direct access, fallback (0).
  // optional_type field_06;

  // Optional, direct access, invariant (>= 0).
  optional_type field_07;

  // // Optional, direct access, fallback (0), invariant (>= 0).
  // optional_type field_08;

  // Variant, direct access.
  variant_type field_09;

  // Variant, direct access, fallback (0).
  variant_type field_10;

  // Variant, direct access, invariant (>= 0).
  variant_type field_11;

  // Variant, direct access, fallback (0), invariant (>= 0).
  variant_type field_12;

  // Tuple, direct access.
  tuple_type field_13;

  // Tuple, direct access, fallback ("", 0).
  tuple_type field_14;

  // Tuple, direct access, invariant (>= 0).
  tuple_type field_15;

  // Tuple, direct access, fallback ("", 0), invariant (>= 0).
  tuple_type field_16;

  // Plain, get/set access.
  ADD_GET_SET_FIELD(int32_t, field_17)

  // Plain, get/set access, fallback (0).
  ADD_GET_SET_FIELD(int32_t, field_18)

  // Plain, get/set access, invariant (>= 0).
  ADD_GET_SET_FIELD(int32_t, field_19)

  // Plain, get/set access, fallback (0), invariant (>= 0).
  ADD_GET_SET_FIELD(int32_t, field_20)

  // Optional, get/set access.
  ADD_GET_SET_FIELD(optional_type, field_21)

  // Optional, get/set access, fallback (0).
  // ADD_GET_SET_FIELD(optional_type, field_22)

  // Optional, get/set access, invariant (>= 0).
  ADD_GET_SET_FIELD(optional_type, field_23)

  // Optional, get/set access, fallback (0), invariant (>= 0).
  // ADD_GET_SET_FIELD(optional_type, field_24)

  // Variant, get/set access.
  ADD_GET_SET_FIELD(variant_type, field_25)

  // Variant, get/set access, fallback (0).
  ADD_GET_SET_FIELD(variant_type, field_26)

  // Variant, get/set access, invariant (>= 0).
  ADD_GET_SET_FIELD(variant_type, field_27)

  // Variant, get/set access, fallback (0), invariant (>= 0).
  ADD_GET_SET_FIELD(variant_type, field_28)

  // Tuple, get/set access.
  ADD_GET_SET_FIELD(tuple_type, field_29)

  // Tuple, get/set access, fallback ("", 0).
  ADD_GET_SET_FIELD(tuple_type, field_30)

  // Tuple, get/set access, invariant (>= 0).
  ADD_GET_SET_FIELD(tuple_type, field_31)

  // Tuple, get/set access, fallback ("", 0), invariant (>= 0).
  ADD_GET_SET_FIELD(tuple_type, field_32)

  // Optional variant, direct access.
  optional_variant_type field_33;

  // Optional tuple, direct access.
  optional_tuple_type field_34;

  // Optional variant, get/set access.
  ADD_GET_SET_FIELD(optional_variant_type, field_35)

  // Optional variant, get/set access.
  ADD_GET_SET_FIELD(optional_tuple_type, field_36)

  // Plain, direct access with custom inspector_access.
  weekday field_37;

  // Plain, get/set access with custom inspector_access.
  ADD_GET_SET_FIELD(weekday, field_38)
};

#undef ADD_GET_SET_FIELD

#define DIRECT_FIELD(num) field("field_" #num, x.field_##num)
#define GET_SET_FIELD(num)                                                     \
  field(                                                                       \
    "field_" #num, [&x]() -> decltype(auto) { return x.field_##num(); },       \
    [&x](auto&& value) {                                                       \
      x.field_##num(std::forward<decltype(value)>(value));                     \
      return true;                                                             \
    })

template <class Inspector>
bool inspect(Inspector& f, nasty& x) {
  using std::get;
  using std::get_if;
  struct {
    bool operator()(int32_t x) {
      return x >= 0;
    }
    bool operator()(const nasty::optional_type& x) {
      return x ? *x >= 0 : true;
    }
    bool operator()(const nasty::variant_type& x) {
      if (auto ptr = get_if<int32_t>(&x))
        return *ptr >= 0;
      return true;
    }
    bool operator()(const nasty::tuple_type& x) {
      return get<1>(x) >= 0;
    }
  } is_positive;
  nasty::variant_type default_variant{int32_t{0}};
  nasty::tuple_type default_tuple{"", 0};
  return f.object(x).fields(                                              //
    f.DIRECT_FIELD(01),                                                   //
    f.DIRECT_FIELD(02).fallback(0),                                       //
    f.DIRECT_FIELD(03).invariant(is_positive),                            //
    f.DIRECT_FIELD(04).fallback(0).invariant(is_positive),                //
    f.DIRECT_FIELD(05),                                                   //
    f.DIRECT_FIELD(07).invariant(is_positive),                            //
    f.DIRECT_FIELD(09),                                                   //
    f.DIRECT_FIELD(10).fallback(default_variant),                         //
    f.DIRECT_FIELD(11).invariant(is_positive),                            //
    f.DIRECT_FIELD(12).fallback(default_variant).invariant(is_positive),  //
    f.DIRECT_FIELD(13),                                                   //
    f.DIRECT_FIELD(14).fallback(default_tuple),                           //
    f.DIRECT_FIELD(15).invariant(is_positive),                            //
    f.DIRECT_FIELD(16).fallback(default_tuple).invariant(is_positive),    //
    f.GET_SET_FIELD(17),                                                  //
    f.GET_SET_FIELD(18).fallback(0),                                      //
    f.GET_SET_FIELD(19).invariant(is_positive),                           //
    f.GET_SET_FIELD(20).fallback(0).invariant(is_positive),               //
    f.GET_SET_FIELD(21),                                                  //
    f.GET_SET_FIELD(23).invariant(is_positive),                           //
    f.GET_SET_FIELD(25),                                                  //
    f.GET_SET_FIELD(26).fallback(default_variant),                        //
    f.GET_SET_FIELD(27).invariant(is_positive),                           //
    f.GET_SET_FIELD(28).fallback(default_variant).invariant(is_positive), //
    f.GET_SET_FIELD(29),                                                  //
    f.GET_SET_FIELD(30).fallback(default_tuple),                          //
    f.GET_SET_FIELD(31).invariant(is_positive),                           //
    f.GET_SET_FIELD(32).fallback(default_tuple).invariant(is_positive),   //
    f.DIRECT_FIELD(33),                                                   //
    f.DIRECT_FIELD(34),                                                   //
    f.GET_SET_FIELD(35),                                                  //
    f.GET_SET_FIELD(36));
}

#undef DIRECT_FIELD
#undef GET_SET_FIELD
