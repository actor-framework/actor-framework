// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/config_value.hpp"

#include "caf/detail/assert.hpp"
#include "caf/detail/config_consumer.hpp"
#include "caf/detail/format.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/detail/overload.hpp"
#include "caf/detail/parse.hpp"
#include "caf/detail/parser/read_config.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/expected.hpp"
#include "caf/format_to_error.hpp"
#include "caf/parser_state.hpp"
#include "caf/pec.hpp"
#include "caf/settings.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ostream>
#include <string_view>

namespace caf {

namespace {

const char* type_names[] = {"none",   "integer",  "boolean",
                            "real",   "timespan", "uri",
                            "string", "list",     "dictionary"};

template <class To, class From>
auto no_conversion() {
  return [](const From&) -> expected<To> {
    return format_to_error(
      sec::conversion_failed, "cannot convert from type {} to type {}",
      type_names[detail::tl_index_of_v<config_value::types, From>],
      type_names[detail::tl_index_of_v<config_value::types, To>]);
  };
}

template <class To, class... From>
auto no_conversions() {
  return detail::make_overload(no_conversion<To, From>()...);
}

template <class T>
constexpr ptrdiff_t signed_index_of() {
  return detail::tl_index_of<typename config_value::types, T>::value;
}

} // namespace

// -- constructors, destructors, and assignment operators ----------------------

config_value::~config_value() {
  // nop
}

// -- parsing ------------------------------------------------------------------

expected<config_value> config_value::parse(std::string_view::iterator first,
                                           std::string_view::iterator last) {
  using namespace detail;
  // Sanity check.
  if (first == last)
    return config_value{""};
  // Skip to beginning of the argument (drop leading whitespaces).
  while (isspace(*first))
    if (++first == last)
      return config_value{""};
  auto i = first;
  // Drop trailing whitespaces.
  while (isspace(*(last - 1)))
    --last;
  // Dispatch to parser.
  detail::config_value_consumer f;
  string_parser_state res{i, last};
  parser::read_config_value(res, f);
  if (res.code == pec::success)
    return std::move(f.result);
  // Assume an unescaped string unless the first character clearly indicates
  // otherwise.
  switch (*i) {
    case '[':
    case '{':
    case '"':
    case '\'':
      return error{res.code};
    default:
      if (isdigit(*i))
        return error{res.code};
      return config_value{std::string{first, last}};
  }
}

expected<config_value> config_value::parse(std::string_view str) {
  return parse(str.begin(), str.end());
}

// -- properties ---------------------------------------------------------------

void config_value::convert_to_list() {
  if (holds_alternative<list>(data_)) {
    ; // nop
  } else if (holds_alternative<none_t>(data_)) {
    data_ = config_value::list{};
  } else {
    using std::swap;
    config_value tmp;
    swap(*this, tmp);
    data_ = config_value::list{std::move(tmp)};
  }
}

config_value::list& config_value::as_list() {
  convert_to_list();
  return get<list>(*this);
}

config_value::dictionary& config_value::as_dictionary() {
  if (auto dict = get_if<config_value::dictionary>(&data_)) {
    return *dict;
  } else if (auto lifted = to_dictionary()) {
    data_ = std::move(*lifted);
    return get<config_value::dictionary>(data_);
  } else {
    data_ = config_value::dictionary{};
    return get<config_value::dictionary>(data_);
  }
}

void config_value::append(config_value x) {
  convert_to_list();
  get<list>(data_).emplace_back(std::move(x));
}

const char* config_value::type_name() const noexcept {
  return type_name_at_index(data_.index());
}

const char* config_value::type_name_at_index(size_t index) noexcept {
  return type_names[index];
}

ptrdiff_t config_value::signed_index() const noexcept {
  return static_cast<ptrdiff_t>(data_.index());
}

// -- utility ------------------------------------------------------------------

type_id_t config_value::type_id() const noexcept {
  static constexpr type_id_t allowed_types[] = {
    type_id_v<none_t>,
    type_id_v<config_value::integer>,
    type_id_v<config_value::boolean>,
    type_id_v<config_value::real>,
    type_id_v<timespan>,
    type_id_v<uri>,
    type_id_v<config_value::string>,
    type_id_v<config_value::list>,
    type_id_v<config_value::dictionary>,
  };
  return allowed_types[data_.index()];
}

error_code<sec> config_value::default_construct(type_id_t id) {
  switch (id) {
    case type_id_v<bool>:
      data_ = false;
      return sec::none;
    case type_id_v<double>:
    case type_id_v<float>:
    case type_id_v<long double>:
      data_ = 0.0;
      return sec::none;
    case type_id_v<int16_t>:
    case type_id_v<int32_t>:
    case type_id_v<int64_t>:
    case type_id_v<int8_t>:
    case type_id_v<uint16_t>:
    case type_id_v<uint32_t>:
    case type_id_v<uint64_t>:
    case type_id_v<uint8_t>:
      data_ = int64_t{0};
      return sec::none;
    case type_id_v<std::string>:
      data_ = std::string{};
      return sec::none;
    case type_id_v<timespan>:
      data_ = timespan{};
      return sec::none;
    case type_id_v<uri>:
      data_ = uri{};
      return sec::none;
    default:
      if (auto meta = detail::global_meta_object_or_null(id)) {
        auto ptr = malloc(meta->padded_size);
        auto free_guard = detail::scope_guard{[ptr]() noexcept { free(ptr); }};
        meta->default_construct(ptr);
        auto destroy_guard
          = detail::scope_guard{[=]() noexcept { meta->destroy(ptr); }};
        config_value_writer writer{this};
        if (meta->save(writer, ptr)) {
          return sec::none;
        } else {
          auto& err = writer.get_error();
          if (err.category() == type_id_v<sec>)
            return static_cast<sec>(err.code());
          else
            return sec::conversion_failed;
        }
      } else {
        return sec::unknown_type;
      }
  }
}

expected<bool> config_value::to_boolean() const {
  using result_type = expected<bool>;
  auto f = detail::make_overload(
    no_conversions<bool, none_t, integer, real, timespan, uri,
                   config_value::list>(),
    [](boolean x) { return result_type{x}; },
    [](const std::string& x) -> result_type {
      if (x == "true") {
        return result_type{true};
      }
      if (x == "false") {
        return result_type{false};
      }
      return format_to_error(sec::conversion_failed,
                             "cannot convert '{}' to a boolean", x);
    },
    [](const dictionary& x) -> result_type {
      if (auto i = x.find("@type");
          i != x.end() && holds_alternative<std::string>(i->second)) {
        const auto& tn = get<std::string>(i->second);
        if (tn == type_name_v<bool>) {
          if (auto j = x.find("value"); j != x.end()) {
            return j->second.to_boolean();
          }
          return format_to_error(sec::conversion_failed,
                                 "missing value for object of type {}", tn);
        }
        return format_to_error(sec::conversion_failed,
                               "cannot convert '{}' to a boolean", tn);
      }
      return format_to_error(sec::conversion_failed,
                             "cannot convert a dictionary to a boolean");
    });
  return visit(f, data_);
}

expected<config_value::integer> config_value::to_integer() const {
  using result_type = expected<integer>;
  auto f = detail::make_overload(
    no_conversions<integer, none_t, bool, timespan, uri, config_value::list>(),
    [](integer x) { return result_type{x}; },
    [](real x) -> result_type {
      using limits = std::numeric_limits<config_value::integer>;
      if (std::isfinite(x)            // never convert NaN & friends
          && std::fmod(x, 1.0) == 0.0 // only convert whole numbers
          && x <= static_cast<config_value::real>(limits::max())
          && x >= static_cast<config_value::real>(limits::min())) {
        return result_type{static_cast<config_value::integer>(x)};
      }
      return format_to_error(sec::conversion_failed,
                             "cannot convert decimal or out-of-bounds "
                             "real number to an integer");
    },
    [](const std::string& x) -> result_type {
      auto tmp_int = config_value::integer{0};
      if (detail::parse(x, tmp_int) == none)
        return result_type{tmp_int};
      auto tmp_real = 0.0;
      if (detail::parse(x, tmp_real) == none)
        if (auto ival = config_value{tmp_real}.to_integer())
          return result_type{*ival};
      return format_to_error(sec::conversion_failed,
                             "cannot convert '{} to an integer", x);
    },
    [](const dictionary& x) -> result_type {
      if (auto i = x.find("@type");
          i != x.end() && holds_alternative<std::string>(i->second)) {
        const auto& tn = get<std::string>(i->second);
        std::string_view valid_types[]
          = {type_name_v<int16_t>,  type_name_v<int32_t>,
             type_name_v<int64_t>,  type_name_v<int8_t>,
             type_name_v<uint16_t>, type_name_v<uint32_t>,
             type_name_v<uint64_t>, type_name_v<uint8_t>};
        auto eq = [&tn](std::string_view x) { return x == tn; };
        if (std::any_of(std::begin(valid_types), std::end(valid_types), eq)) {
          if (auto j = x.find("value"); j != x.end()) {
            return j->second.to_integer();
          }
          return format_to_error(sec::conversion_failed,
                                 "missing value for object of type {}", tn);
        }
        return format_to_error(sec::conversion_failed,
                               "cannot convert {} to an integer", tn);
      }
      return format_to_error(sec::conversion_failed,
                             "cannot convert a dictionary to an integer");
    });
  return visit(f, data_);
}

expected<config_value::real> config_value::to_real() const {
  using result_type = expected<real>;
  auto f = detail::make_overload(
    no_conversions<real, none_t, bool, timespan, uri, config_value::list>(),
    [](integer x) {
      // This cast may lose precision on the value. We could try and check that,
      // but refusing to convert on loss of precision could also be unexpected
      // behavior. So we rather always convert, even if it costs precision.
      return result_type{static_cast<real>(x)};
    },
    [](real x) { return result_type{x}; },
    [](const std::string& x) -> result_type {
      auto tmp = 0.0;
      if (detail::parse(x, tmp) == none)
        return result_type{tmp};
      return format_to_error(sec::conversion_failed,
                             "cannot convert {} to a floating point number", x);
    },
    [](const dictionary& x) -> result_type {
      if (auto i = x.find("@type");
          i != x.end() && holds_alternative<std::string>(i->second)) {
        const auto& tn = get<std::string>(i->second);
        std::string_view valid_types[]
          = {type_name_v<float>, type_name_v<double>, type_name_v<long double>};
        auto eq = [&tn](std::string_view x) { return x == tn; };
        if (std::any_of(std::begin(valid_types), std::end(valid_types), eq)) {
          if (auto j = x.find("value"); j != x.end()) {
            return j->second.to_real();
          }
          return format_to_error(sec::conversion_failed,
                                 "missing value for object of type {}", tn);
        }
        return format_to_error(sec::conversion_failed,
                               "cannot convert {} to a floating point number",
                               tn);
      }
      return format_to_error(sec::conversion_failed,
                             "cannot convert a dictionary to "
                             "a floating point number");
    });
  return visit(f, data_);
}

expected<timespan> config_value::to_timespan() const {
  using result_type = expected<timespan>;
  auto f = detail::make_overload(
    no_conversions<timespan, none_t, bool, integer, real, uri,
                   config_value::list, config_value::dictionary>(),
    [](timespan x) { return result_type{x}; },
    [](const std::string& x) -> result_type {
      auto tmp = timespan{};
      if (detail::parse(x, tmp) == none)
        return result_type{tmp};
      return format_to_error(sec::conversion_failed,
                             "cannot convert '{}' to a timespan", x);
    });
  return visit(f, data_);
}

expected<uri> config_value::to_uri() const {
  using result_type = expected<uri>;
  auto f = detail::make_overload(
    no_conversions<uri, none_t, bool, integer, real, timespan,
                   config_value::list, config_value::dictionary>(),
    [](const uri& x) { return result_type{x}; },
    [](const std::string& x) { return make_uri(x); });
  return visit(f, data_);
}

expected<config_value::list> config_value::to_list() const {
  using result_type = expected<list>;
  auto dict_to_list = [](const dictionary& dict, list& result) {
    for (const auto& [key, val] : dict) {
      list kvp;
      kvp.reserve(2);
      kvp.emplace_back(key);
      kvp.emplace_back(val);
      result.emplace_back(std::move(kvp));
    }
  };
  auto f = detail::make_overload(
    no_conversions<list, none_t, bool, integer, real, timespan, uri>(),
    [dict_to_list](const std::string& x) -> result_type {
      // Check whether we can parse the string as a list. However, we also
      // accept dictionaries that we convert to lists of key-value pairs. We
      // need to try converting to dictionary *first*, because detail::parse for
      // the list otherwise produces a list with one dictionary.
      if (config_value::dictionary dict; detail::parse(x, dict) == none) {
        config_value::list tmp;
        dict_to_list(dict, tmp);
        return result_type{std::move(tmp)};
      }
      if (config_value::list tmp; detail::parse(x, tmp) == none)
        return result_type{std::move(tmp)};
      return format_to_error(sec::conversion_failed,
                             "cannot convert '{}' to a list", x);
    },
    [](const list& x) { return result_type{x}; },
    [dict_to_list](const dictionary& x) {
      list tmp;
      dict_to_list(x, tmp);
      return result_type{std::move(tmp)};
    });
  return visit(f, data_);
}

expected<config_value::dictionary> config_value::to_dictionary() const {
  using result_type = expected<dictionary>;
  auto f = detail::make_overload(
    no_conversions<dictionary, none_t, bool, integer, timespan, real, uri>(),
    [](const list& x) -> result_type {
      dictionary tmp;
      auto lift = [&tmp](const config_value& element) {
        auto ls = element.to_list();
        if (ls && ls->size() == 2)
          return tmp.emplace(to_string((*ls)[0]), std::move((*ls)[1])).second;
        else
          return false;
      };
      if (std::all_of(x.begin(), x.end(), lift)) {
        return result_type{std::move(tmp)};
      }
      return error{sec::conversion_failed,
                   "cannot convert list to dictionary unless each "
                   "element in the list is a key-value pair"};
    },
    [this](const std::string& x) -> result_type {
      if (dictionary tmp; detail::parse(x, tmp) == none)
        return result_type{std::move(tmp)};
      if (auto lst = to_list()) {
        config_value ls{std::move(*lst)};
        if (auto res = ls.to_dictionary())
          return res;
      }
      return format_to_error(sec::conversion_failed,
                             "cannot convert '{}' to a dictionary", x);
    },
    [](const dictionary& x) { return result_type{x}; });
  return visit(f, data_);
}

bool config_value::can_convert_to_dictionary() const {
  auto f = detail::make_overload( //
    [](const auto&) { return false; },
    [this](const std::string&) {
      // TODO: implement some dry-run mode and use it here to avoid creating
      // an actual dictionary only to throw it away.
      auto maybe_dict = to_dictionary();
      return maybe_dict.has_value();
    },
    [](const dictionary&) { return true; });
  return visit(f, data_);
}

std::optional<message>
config_value::parse_msg_impl(std::string_view str,
                             span<const type_id_list> allowed_types) {
  if (auto val = parse(str)) {
    auto ls_size = val->as_list().size();
    message result;
    auto converts = [&val, &result, ls_size](type_id_list ls) {
      if (ls.size() != ls_size)
        return false;
      config_value_reader reader{std::addressof(*val)};
      auto unused = size_t{0};
      reader.begin_sequence(unused);
      CAF_ASSERT(unused == ls_size);
      intrusive_ptr<detail::message_data> ptr;
      if (auto vptr = malloc(sizeof(detail::message_data) + ls.data_size()))
        ptr.reset(new (vptr) detail::message_data(ls), false);
      else
        return false;
      auto pos = ptr->storage();
      for (auto type : ls) {
        auto& meta = detail::global_meta_object(type);
        meta.default_construct(pos);
        ptr->inc_constructed_elements();
        if (!meta.load(reader, pos))
          return false;
        pos += meta.padded_size;
      }
      result.reset(ptr.release(), false);
      return reader.end_sequence();
    };
    if (std::any_of(allowed_types.begin(), allowed_types.end(), converts))
      return {std::move(result)};
  }
  return std::nullopt;
}

// -- related free functions ---------------------------------------------------

bool operator<(double x, const config_value& y) {
  return config_value{x} < y;
}

bool operator<=(double x, const config_value& y) {
  return config_value{x} <= y;
}

bool operator==(double x, const config_value& y) {
  return config_value{x} == y;
}

bool operator>(double x, const config_value& y) {
  return config_value{x} > y;
}

bool operator>=(double x, const config_value& y) {
  return config_value{x} >= y;
}

bool operator<(const config_value& x, double y) {
  return x < config_value{y};
}

bool operator<=(const config_value& x, double y) {
  return x <= config_value{y};
}

bool operator==(const config_value& x, double y) {
  return x == config_value{y};
}

bool operator>(const config_value& x, double y) {
  return x > config_value{y};
}

bool operator>=(const config_value& x, double y) {
  return x >= config_value{y};
}

bool operator<(const config_value& x, const config_value& y) {
  return x.get_data() < y.get_data();
}

bool operator<=(const config_value& x, const config_value& y) {
  return x.get_data() <= y.get_data();
}

bool operator==(const config_value& x, const config_value& y) {
  return x.get_data() == y.get_data();
}

bool operator>(const config_value& x, const config_value& y) {
  return x.get_data() > y.get_data();
}

bool operator>=(const config_value& x, const config_value& y) {
  return x.get_data() >= y.get_data();
}

namespace {
void to_string_impl(std::string& str, const config_value& x);

struct to_string_visitor {
  std::string& str;

  void operator()(const std::string& x) {
    detail::print_escaped(str, x);
  }

  template <class T>
  void operator()(const T& x) {
    detail::print(str, x);
  }

  void operator()(none_t) {
    str += "null";
  }

  void operator()(const uri& x) {
    auto x_str = x.str();
    str.insert(str.end(), x_str.begin(), x_str.end());
  }

  void operator()(const config_value::list& xs) {
    str += '[';
    if (!xs.empty()) {
      auto i = xs.begin();
      to_string_impl(str, *i);
      for (++i; i != xs.end(); ++i) {
        str += ", ";
        to_string_impl(str, *i);
      }
    }
    str += ']';
  }

  void append_key(const std::string& key) {
    if (std::all_of(key.begin(), key.end(), ::isalnum))
      str.append(key.begin(), key.end());
    else
      (*this)(key);
  }

  void operator()(const config_value::dictionary& xs) {
    str += '{';
    if (!xs.empty()) {
      auto i = xs.begin();
      append_key(i->first);
      str += " = ";
      to_string_impl(str, i->second);
      for (++i; i != xs.end(); ++i) {
        str += ", ";
        append_key(i->first);
        str += " = ";
        to_string_impl(str, i->second);
      }
    }
    str += '}';
  }
};

void to_string_impl(std::string& str, const config_value& x) {
  to_string_visitor f{str};
  visit(f, x.get_data());
}

} // namespace

std::string to_string(const config_value& x) {
  if (auto str = get_if<std::string>(std::addressof(x.get_data()))) {
    return *str;
  } else {
    std::string result;
    to_string_impl(result, x);
    return result;
  }
}

std::string to_string(const settings& xs) {
  std::string result;
  to_string_visitor f{result};
  f(xs);
  return result;
}

std::ostream& operator<<(std::ostream& out, const config_value& x) {
  return out << to_string(x);
}

} // namespace caf
