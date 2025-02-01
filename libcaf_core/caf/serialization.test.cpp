// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/approx.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/outline.hpp"
#include "caf/test/runnable.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/config_value_reader.hpp"
#include "caf/config_value_writer.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/json_reader.hpp"
#include "caf/json_writer.hpp"
#include "caf/raise_error.hpp"
#include "caf/serializer.hpp"
#include "caf/string_algorithms.hpp"

#include <regex>
#include <unordered_set>
#include <variant>

using namespace std::literals;

namespace {

enum class weekday : uint8_t;
class nasty;
struct c_array;

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(serialization_test, caf::first_custom_type_id + 25)

  CAF_ADD_TYPE_ID(serialization_test, (nasty))
  CAF_ADD_TYPE_ID(serialization_test, (weekday))
  CAF_ADD_TYPE_ID(serialization_test, (c_array))

CAF_END_TYPE_ID_BLOCK(serialization_test)

namespace {

enum class weekday : uint8_t {
  monday,
  tuesday,
  wednesday,
  thursday,
  friday,
  saturday,
  sunday,
};

std::string to_string(weekday x) {
  switch (x) {
    default:
      return "???";
    case weekday::monday:
      return "monday";
    case weekday::tuesday:
      return "tuesday";
    case weekday::wednesday:
      return "wednesday";
    case weekday::thursday:
      return "thursday";
    case weekday::friday:
      return "friday";
    case weekday::saturday:
      return "saturday";
    case weekday::sunday:
      return "sunday";
  }
}

bool parse(std::string_view input, weekday& dest) {
  if (input == "monday") {
    dest = weekday::monday;
    return true;
  }
  if (input == "tuesday") {
    dest = weekday::tuesday;
    return true;
  }
  if (input == "wednesday") {
    dest = weekday::wednesday;
    return true;
  }
  if (input == "thursday") {
    dest = weekday::thursday;
    return true;
  }
  if (input == "friday") {
    dest = weekday::friday;
    return true;
  }
  if (input == "saturday") {
    dest = weekday::saturday;
    return true;
  }
  if (input == "sunday") {
    dest = weekday::sunday;
    return true;
  }
  return false;
}

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

struct c_array {
  int32_t value[4];

  bool operator==(const c_array& other) const {
    return std::equal(std::begin(value), std::end(value),
                      std::begin(other.value));
  }
};

template <class Inspector>
bool inspect(Inspector& f, c_array& x) {
  return f.object(x).fields(f.field("value", x.value));
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
  weekday field_37 = weekday::monday;

  // Plain, get/set access with custom inspector_access.
  ADD_GET_SET_FIELD(weekday, field_38)
};

bool operator==(const nasty& lhs, const nasty& rhs) {
  return lhs.field_01 == rhs.field_01        //
         && lhs.field_02 == rhs.field_02     //
         && lhs.field_03 == rhs.field_03     //
         && lhs.field_04 == rhs.field_04     //
         && lhs.field_05 == rhs.field_05     //
         && lhs.field_07 == rhs.field_07     //
         && lhs.field_09 == rhs.field_09     //
         && lhs.field_10 == rhs.field_10     //
         && lhs.field_11 == rhs.field_11     //
         && lhs.field_12 == rhs.field_12     //
         && lhs.field_13 == rhs.field_13     //
         && lhs.field_14 == rhs.field_14     //
         && lhs.field_15 == rhs.field_15     //
         && lhs.field_16 == rhs.field_16     //
         && lhs.field_17() == rhs.field_17() //
         && lhs.field_18() == rhs.field_18() //
         && lhs.field_19() == rhs.field_19() //
         && lhs.field_20() == rhs.field_20() //
         && lhs.field_21() == rhs.field_21() //
         && lhs.field_23() == rhs.field_23() //
         && lhs.field_25() == rhs.field_25() //
         && lhs.field_26() == rhs.field_26() //
         && lhs.field_27() == rhs.field_27() //
         && lhs.field_28() == rhs.field_28() //
         && lhs.field_29() == rhs.field_29() //
         && lhs.field_30() == rhs.field_30() //
         && lhs.field_31() == rhs.field_31() //
         && lhs.field_32() == rhs.field_32() //
         && lhs.field_33 == rhs.field_33     //
         && lhs.field_34 == rhs.field_34     //
         && lhs.field_35() == rhs.field_35() //
         && lhs.field_36() == rhs.field_36() //
         && lhs.field_37 == rhs.field_37     //
         && lhs.field_38() == rhs.field_38();
}

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
    f.GET_SET_FIELD(36),                                                  //
    f.DIRECT_FIELD(37),                                                   //
    f.GET_SET_FIELD(38));                                                 //
}

} // namespace

using namespace caf;

namespace {

using deserializer_ptr = std::variant<std::shared_ptr<binary_deserializer>,
                                      std::shared_ptr<json_reader>,
                                      std::shared_ptr<config_value_reader>>;

struct binary_serializer_wrapper {
  byte_buffer buffer;
  binary_serializer sink;

  explicit binary_serializer_wrapper(actor_system& sys) : sink(sys, buffer) {
    // nop
  }

  deserializer_ptr make_deserializer() {
    return std::make_shared<binary_deserializer>(*sink.context(), buffer);
  }
};

struct json_writer_wrapper {
  json_writer sink;

  explicit json_writer_wrapper(actor_system& sys) : sink(sys) {
    // nop
  }

  deserializer_ptr make_deserializer() {
    auto reader = std::make_shared<json_reader>(*sink.sys());
    if (!reader->load(sink.str())) {
      test::runnable::current().fail("failed to load JSON: {}",
                                     reader->get_error());
    }
    return reader;
  }
};

struct config_value_writer_wrapper {
  config_value value;
  config_value_writer sink;

  explicit config_value_writer_wrapper(actor_system&) : sink(&value) {
    // nop
  }

  deserializer_ptr make_deserializer() {
    return std::make_shared<config_value_reader>(&value);
  }
};

using str_list = std::vector<std::string>;
using serializer_wrapper
  = std::variant<std::shared_ptr<binary_serializer_wrapper>,
                 std::shared_ptr<json_writer_wrapper>,
                 std::shared_ptr<config_value_writer_wrapper>>;

struct fixture : caf::test::fixture::deterministic {
  serializer_wrapper serializer_by_name(const std::string& name) {
    if (name == "binary_serializer") {
      return std::make_shared<binary_serializer_wrapper>(sys);
    }
    if (name == "json_writer") {
      return std::make_shared<json_writer_wrapper>(sys);
    }
    if (name == "config_value_writer") {
      return std::make_shared<config_value_writer_wrapper>(sys);
    }
    CAF_RAISE_ERROR(std::logic_error, "invalid serializer name");
  }

  auto make_deserializer(serializer_wrapper& from) {
    return std::visit([&](auto& ptr) { return ptr->make_deserializer(); },
                      from);
  }

  std::variant<
    int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t,
    double, long double, float, std::string, std::u16string, std::u32string,
    std::vector<int32_t>, std::vector<bool>, std::list<int32_t>,
    std::map<std::string, int32_t>, std::unordered_map<std::string, int32_t>,
    std::set<int32_t>, std::unordered_set<int32_t>, std::array<int32_t, 5>,
    std::tuple<int32_t, std::string, int32_t>, c_array>
  read_val(const std::string& type, const std::string& value) {
    str_list parsed_value;
    if (type == "i8") {
      return static_cast<int8_t>(std::stoi(value));
    }
    if (type == "i16") {
      return static_cast<int16_t>(std::stoi(value));
    }
    if (type == "i32") {
      return static_cast<int32_t>(std::stoi(value));
    }
    if (type == "i64") {
      return static_cast<int64_t>(std::stoll(value));
    }
    if (type == "u8") {
      return static_cast<uint8_t>(std::stoul(value));
    }
    if (type == "u16") {
      return static_cast<uint16_t>(std::stoul(value));
    }
    if (type == "u32") {
      return static_cast<uint32_t>(std::stoul(value));
    }
    if (type == "u64") {
      return static_cast<uint64_t>(std::stoull(value));
    }
    if (type == "ld") {
      return static_cast<long double>(std::stold(value));
    }
    if (type == "f") {
      return static_cast<float>(std::stof(value));
    }
    if (type == "real") {
      return std::stod(value);
    }
    if (type == "string") {
      return value;
    }
    if (type == "u16str") {
      return std::u16string{value.begin(), value.end()};
    }
    if (type == "u32str") {
      return std::u32string{value.begin(), value.end()};
    }
    if (type == "vector") {
      std::vector<int32_t> ivec;
      caf::split(parsed_value, value, ",");
      std::transform(parsed_value.cbegin(), parsed_value.cend(),
                     std::back_inserter(ivec),
                     [](const std::string& s) { return std::stoi(s); });
      return ivec;
    }
    if (type == "v_bool") {
      std::vector<bool> ivec;
      caf::split(parsed_value, value, ",");
      std::transform(parsed_value.cbegin(), parsed_value.cend(),
                     std::back_inserter(ivec),
                     [](const std::string& s) { return s == "true"; });
      return ivec;
    }
    if (type == "list") {
      std::list<int32_t> ilist;
      caf::split(parsed_value, value, ",");
      std::transform(parsed_value.cbegin(), parsed_value.cend(),
                     std::back_inserter(ilist),
                     [](const std::string& s) { return std::stoi(s); });
      return ilist;
    }
    if (type == "map") {
      std::map<std::string, int32_t> imap;
      caf::split(parsed_value, value, ",");
      std::transform(parsed_value.cbegin(), parsed_value.cend(),
                     std::inserter(imap, imap.begin()),
                     [](const std::string& s) {
                       str_list parsed_pair;
                       caf::split(parsed_pair, s, ":");
                       return std::pair{parsed_pair[0],
                                        std::stoi(parsed_pair[1])};
                     });
      return imap;
    }
    if (type == "umap") {
      std::unordered_map<std::string, int32_t> iumap;
      caf::split(parsed_value, value, ",");
      std::transform(parsed_value.cbegin(), parsed_value.cend(),
                     std::inserter(iumap, iumap.begin()),
                     [](const std::string& s) {
                       str_list parsed_pair;
                       caf::split(parsed_pair, s, ":");
                       return std::pair{parsed_pair[0],
                                        std::stoi(parsed_pair[1])};
                     });
      return iumap;
    }
    if (type == "set") {
      std::set<int32_t> iset;
      caf::split(parsed_value, value, ",");
      std::transform(parsed_value.cbegin(), parsed_value.cend(),
                     std::inserter(iset, iset.begin()),
                     [](const std::string& s) { return std::stoi(s); });
      return iset;
    }
    if (type == "uset") {
      std::unordered_set<int32_t> iuset;
      caf::split(parsed_value, value, ",");
      std::transform(parsed_value.cbegin(), parsed_value.cend(),
                     std::inserter(iuset, iuset.begin()),
                     [](const std::string& s) { return std::stoi(s); });
      return iuset;
    }
    if (type == "array") {
      std::array<int, 5> iarray;
      caf::split(parsed_value, value, ",");
      if (parsed_value.size() > 5) {
        CAF_RAISE_ERROR(std::logic_error, "invalid array size");
      }
      std::transform(parsed_value.cbegin(), parsed_value.cend(), iarray.begin(),
                     [](const std::string& s) { return std::stoi(s); });
      std::fill(iarray.begin() + parsed_value.size(), iarray.end(), 0);
      return iarray;
    }
    if (type == "tuple") {
      str_list parsed_value;
      caf::split(parsed_value, value, ",");
      if (parsed_value.size() != 3) {
        CAF_RAISE_ERROR(std::logic_error, "invalid tuple size");
      }
      return std::tuple{std::stoi(parsed_value[0]), parsed_value[1],
                        std::stoi(parsed_value[2])};
    }
    if (type == "carray") {
      c_array ic_array;
      caf::split(parsed_value, value, ",");
      if (parsed_value.size() != 4) {
        CAF_RAISE_ERROR(std::logic_error, "invalid c array size");
      }
      std::transform(parsed_value.cbegin(), parsed_value.cend(),
                     std::begin(ic_array.value),
                     [](const std::string& s) { return std::stoi(s); });
      return ic_array;
    }
    CAF_RAISE_ERROR(std::logic_error, "invalid type");
  }

  std::variant<
    int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t,
    double, long double, float, std::string, std::u16string, std::u32string,
    std::vector<int32_t>, std::vector<bool>, std::list<int32_t>,
    std::map<std::string, int32_t>, std::unordered_map<std::string, int32_t>,
    std::set<int32_t>, std::unordered_set<int32_t>, std::array<int32_t, 5>,
    std::tuple<int32_t, std::string, int32_t>, c_array>
  default_val(const std::string& type) {
    if (type == "i8") {
      return static_cast<int8_t>(0);
    }
    if (type == "i16") {
      return static_cast<int16_t>(0);
    }
    if (type == "i32") {
      return static_cast<int32_t>(0);
    }
    if (type == "i64") {
      return static_cast<int64_t>(0);
    }
    if (type == "u8") {
      return static_cast<uint8_t>(0);
    }
    if (type == "u16") {
      return static_cast<uint16_t>(0);
    }
    if (type == "u32") {
      return static_cast<uint32_t>(0);
    }
    if (type == "u64") {
      return static_cast<uint64_t>(0);
    }
    if (type == "ld") {
      return static_cast<long double>(0);
    }
    if (type == "f") {
      return static_cast<float>(0);
    }
    if (type == "real") {
      return 0.0;
    }
    if (type == "string") {
      return "";
    }
    if (type == "u16str") {
      return u"";
    }
    if (type == "u32str") {
      return U"";
    }
    if (type == "vector") {
      return std::vector<int32_t>{};
    }
    if (type == "v_bool") {
      return std::vector<bool>{};
    }
    if (type == "list") {
      return std::list<int32_t>{};
    }
    if (type == "map") {
      return std::map<std::string, int32_t>{};
    }
    if (type == "umap") {
      return std::unordered_map<std::string, int32_t>{};
    }
    if (type == "set") {
      return std::set<int32_t>{};
    }
    if (type == "uset") {
      return std::unordered_set<int32_t>{};
    }
    if (type == "array") {
      return std::array<int32_t, 5>{};
    }
    if (type == "tuple") {
      return std::tuple<int32_t, std::string, int32_t>{};
    }
    if (type == "carray") {
      return c_array{};
    }
    CAF_RAISE_ERROR(std::logic_error, "invalid type");
  }
};

} // namespace

WITH_FIXTURE(fixture) {

OUTLINE("serializing and then deserializing primitive values") {
  GIVEN("a <serializer>") {
    auto sink = serializer_by_name(block_parameters<std::string>());
    WHEN("serializing '<value>' of type <type>") {
      auto [val_str, val_type] = block_parameters<std::string, std::string>();
      auto value = read_val(val_type, val_str);
      std::visit([this](auto& ptr, auto& val) { check(ptr->sink.apply(val)); },
                 sink, value);
      THEN("deserializing the result produces the value again") {
        auto source = make_deserializer(sink);
        auto copy = default_val(val_type);
        std::visit([this](auto& ptr, auto& val) { check(ptr->apply(val)); },
                   source, copy);
        std::visit(
          [this](auto& lhs, auto& rhs) {
            using lhs_t = std::decay_t<decltype(lhs)>;
            using rhs_t = std::decay_t<decltype(rhs)>;
            if constexpr (std::is_same_v<lhs_t, rhs_t>) {
              if constexpr (std::is_arithmetic_v<lhs_t>
                            || std::is_arithmetic_v<rhs_t>)
                check_eq(lhs, test::approx{rhs});
              else
                check_eq(lhs, rhs);
            } else {
              fail("type mismatch");
            }
          },
          copy, value);
      }
    }
  }
  EXAMPLES = R"_(
    | serializer          | type   | value         |
    | binary_serializer   | i8     | -7            |
    | binary_serializer   | i16    | -999          |
    | binary_serializer   | i32    | -123456       |
    | binary_serializer   | i64    | -123456789    |
    | binary_serializer   | u8     | 42            |
    | binary_serializer   | u16    | 1024          |
    | binary_serializer   | u32    | 123456        |
    | binary_serializer   | u64    | 123456789     |
    | binary_serializer   | ld     | 123.5         |
    | binary_serializer   | f      | 3.14          |
    | binary_serializer   | real   | 12.5          |
    | binary_serializer   | string | Hello, world! |
    | binary_serializer   | u16str | Hello, world! |
    | binary_serializer   | u32str | Hello, world! |
    | binary_serializer   | vector | 1, 42, -31    |
    | binary_serializer   | v_bool | true, false   |
    | binary_serializer   | list   | 1, 42, -31    |
    | binary_serializer   | map    | a:-1, b:42    |
    | binary_serializer   | umap   | a:-1, b:42    |
    | binary_serializer   | set    | 1, -42, 3, 3  |
    | binary_serializer   | uset   | 1, -42, 3, 3  |
    | binary_serializer   | array  | 1, -42, 3     |
    | binary_serializer   | tuple  | -42, 1024, 30 |
    | binary_serializer   | carray | -42, 1, 9, 30 |
    | json_writer         | i8     | -7            |
    | json_writer         | i16    | -999          |
    | json_writer         | i32    | -123456       |
    | json_writer         | i64    | -123456789    |
    | json_writer         | u8     | 42            |
    | json_writer         | u16    | 1024          |
    | json_writer         | u32    | 123456        |
    | json_writer         | u64    | 123456789     |
    | json_writer         | ld     | 123.5         |
    | json_writer         | f      | 3.14          |
    | json_writer         | real   | 12.5          |
    | json_writer         | string | Hello, world! |
    | json_writer         | vector | 1, 42, -31    |
    | json_writer         | v_bool | true, false   |
    | json_writer         | list   | 1, 42, -31    |
    | json_writer         | map    | a:-1, b:42    |
    | json_writer         | umap   | a:-1, b:42    |
    | json_writer         | set    | 1, -42, 3, 3  |
    | json_writer         | uset   | 1, -42, 3, 3  |
    | json_writer         | array  | 1, -42, 3     |
    | json_writer         | tuple  | -42, 1024, 30 |
    | json_writer         | carray | -42, 1, 9, 30 |
    | config_value_writer | i8     | -7            |
    | config_value_writer | i16    | -999          |
    | config_value_writer | i32    | -123456       |
    | config_value_writer | i64    | -123456789    |
    | config_value_writer | u8     | 42            |
    | config_value_writer | u16    | 1024          |
    | config_value_writer | u32    | 123456        |
    | config_value_writer | u64    | 123456789     |
    | config_value_writer | ld     | 123.5         |
    | config_value_writer | f      | 3.14          |
    | config_value_writer | real   | 12.5          |
    | config_value_writer | string | Hello, world! |
    | config_value_writer | vector | 1, 42, -31    |
    | config_value_writer | v_bool | true, false   |
    | config_value_writer | list   | 1, 42, -31    |
    | config_value_writer | map    | a:-1, b:42    |
    | config_value_writer | umap   | a:-1, b:42    |
    | config_value_writer | set    | 1, -42, 3, 3  |
    | config_value_writer | uset   | 1, -42, 3, 3  |
    | config_value_writer | array  | 1, -42, 3     |
    | config_value_writer | tuple  | -42, 1024, 30 |
    | config_value_writer | carray | -42, 1, 9, 30 |
  )_";
}

OUTLINE("serializing and then deserializing the 'nasty' type") {
  GIVEN("a <serializer>") {
    auto sink = serializer_by_name(block_parameters<std::string>());
    WHEN("serializing a value of type 'nasty'") {
      auto val = nasty{};
      val.field_01 = 1;
      val.field_02 = 2;
      val.field_03 = 3;
      val.field_04 = 4;
      val.field_07 = 7;
      val.field_09 = "hello"s;
      val.field_10 = 10;
      val.field_13 = std::tuple{"world"s, 13};
      val.field_17(17);
      val.field_21(21);
      val.field_29(std::tuple{"world"s, 21});
      val.field_37 = weekday::tuesday;
      std::visit([this, &val](auto& ptr) { check(inspect(ptr->sink, val)); },
                 sink);
      THEN("deserializing the result produces the value again") {
        auto source = make_deserializer(sink);
        auto copy = nasty{};
        std::visit([this, &copy](auto& ptr) { check(inspect(*ptr, copy)); },
                   source);
        check_eq(copy, val);
      }
    }
  }
  EXAMPLES = R"_(
    | serializer          |
    | binary_serializer   |
    | json_writer         |
    | config_value_writer |
  )_";
}

struct user {
  uint32_t id = 42;
  std::string name = "John Doe";
};

bool operator==(const user& lhs, const user& rhs) {
  return lhs.id == rhs.id && lhs.name == rhs.name;
}

template <class Inspector>
bool inspect(Inspector& f, user& x) {
  return f.object(x).fields(f.field("id", x.id), f.field("name", x.name));
}

OUTLINE("serializing a type that initializes members to a non-empty state") {
  GIVEN("a <serializer> and a user object") {
    auto sink = serializer_by_name(block_parameters<std::string>());
    auto val = user{};
    val.id = 123;
    val.name = "Alice";
    WHEN("serializing the user object") {
      std::visit([this, &val](auto& ptr) { check(ptr->sink.apply(val)); },
                 sink);
      THEN("deserializing the result produces the value again") {
        auto source = make_deserializer(sink);
        auto copy = user{};
        std::visit([this, &copy](auto& ptr) { check(ptr->apply(copy)); },
                   source);
        check_eq(copy, val);
      }
    }
  }
  EXAMPLES = R"_(
    | serializer          |
    | binary_serializer   |
    | json_writer         |
    | config_value_writer |
  )_";
}

// TODO: update tests to use apply instead of value overload
SCENARIO("binary serializer and deserializer handle vectors of booleans") {
  GIVEN("a binary serializer") {
    auto sink = binary_serializer_wrapper{sys};
    WHEN("serializing a vector of booleans with 8 values") {
      auto val = std::vector<bool>{true,  false, true, true,
                                   false, false, true, false};
      check(sink.sink.value(val));
      THEN("deserializing the result produces the value again") {
        auto source = binary_deserializer{sys, sink.buffer};
        auto copy = std::vector<bool>{};
        check(source.value(copy));
        check_eq(copy, val);
      }
    }
    WHEN("serializing a vector of boolean with 9 values") {
      auto val = std::vector<bool>{true,  false, true,  true, false,
                                   false, true,  false, true};
      check(sink.sink.value(val));
      THEN("deserializing the result produces the value again") {
        auto source = binary_deserializer{sys, sink.buffer};
        auto copy = std::vector<bool>{};
        check(source.value(copy));
        check_eq(copy, val);
      }
    }
    WHEN("serializing a vector of boolean with 1 value") {
      auto val = std::vector<bool>{true};
      check(sink.sink.value(val));
      THEN("deserializing the result produces the value again") {
        auto source = binary_deserializer{sys, sink.buffer};
        auto copy = std::vector<bool>{};
        check(source.value(copy));
        check_eq(copy, val);
      }
    }
    WHEN("serializing an empty vector of boolean") {
      auto val = std::vector<bool>{};
      check(sink.sink.value(val));
      THEN("deserializing the result produces the value again") {
        auto source = binary_deserializer{sys, sink.buffer};
        auto copy = std::vector<bool>{};
        check(source.value(copy));
        check_eq(copy, val);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

TEST_INIT() {
  init_global_meta_objects<id_block::serialization_test>();
}
