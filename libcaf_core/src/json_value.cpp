// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/json_value.hpp"

#include "caf/expected.hpp"
#include "caf/json_array.hpp"
#include "caf/json_object.hpp"
#include "caf/make_counted.hpp"
#include "caf/parser_state.hpp"

#include <fstream>

namespace caf {

// -- conversion ---------------------------------------------------------------

int64_t json_value::to_integer(int64_t fallback) const {
  if (is_integer()) {
    return std::get<int64_t>(val_->data);
  }
  if (is_double()) {
    return static_cast<int64_t>(std::get<double>(val_->data));
  }
  return fallback;
}

double json_value::to_double(double fallback) const {
  if (is_double()) {
    return std::get<double>(val_->data);
  }
  if (is_integer()) {
    return static_cast<double>(std::get<int64_t>(val_->data));
  }
  return fallback;
}

bool json_value::to_bool(bool fallback) const {
  if (is_bool()) {
    return std::get<bool>(val_->data);
  }
  return fallback;
}

std::string_view json_value::to_string() const {
  return to_string(std::string_view{});
}

std::string_view json_value::to_string(std::string_view fallback) const {
  if (is_string()) {
    return std::get<std::string_view>(val_->data);
  }
  return fallback;
}

json_object json_value::to_object() const {
  return to_object(json_object{});
}

json_object json_value::to_object(json_object fallback) const {
  if (is_object()) {
    return json_object{&std::get<detail::json::object>(val_->data), storage_};
  }
  return fallback;
}

json_array json_value::to_array() const {
  return to_array(json_array{});
}

json_array json_value::to_array(json_array fallback) const {
  if (is_array()) {
    return json_array{&std::get<detail::json::array>(val_->data), storage_};
  }
  return fallback;
}

// -- comparison ---------------------------------------------------------------

bool json_value::equal_to(const json_value& other) const noexcept {
  if (val_ == other.val_) {
    return true;
  }
  if (val_ != nullptr && other.val_ != nullptr) {
    return *val_ == *other.val_;
  }
  return false;
}

// -- parsing ------------------------------------------------------------------

expected<json_value> json_value::parse(std::string_view str) {
  auto storage = make_counted<detail::json::storage>();
  string_parser_state ps{str.begin(), str.end()};
  auto root = detail::json::parse(ps, &storage->buf);
  if (ps.code == pec::success)
    return {json_value{root, std::move(storage)}};
  return {make_error(ps)};
}

expected<json_value> json_value::parse_shallow(std::string_view str) {
  auto storage = make_counted<detail::json::storage>();
  string_parser_state ps{str.begin(), str.end()};
  auto root = detail::json::parse_shallow(ps, &storage->buf);
  if (ps.code == pec::success)
    return {json_value{root, std::move(storage)}};
  return {make_error(ps)};
}

expected<json_value> json_value::parse_in_situ(std::string& str) {
  auto storage = make_counted<detail::json::storage>();
  detail::json::mutable_string_parser_state ps{str.data(),
                                               str.data() + str.size()};
  auto root = detail::json::parse_in_situ(ps, &storage->buf);
  if (ps.code == pec::success)
    return {json_value{root, std::move(storage)}};
  return {make_error(ps)};
}

expected<json_value> json_value::parse_file(const char* path) {
  using iterator_t = std::istreambuf_iterator<char>;
  std::ifstream input{path};
  if (!input.is_open())
    return make_error(sec::cannot_open_file);
  auto storage = make_counted<detail::json::storage>();
  detail::json::file_parser_state ps{iterator_t{input}, iterator_t{}};
  auto root = detail::json::parse(ps, &storage->buf);
  if (ps.code == pec::success)
    return {json_value{root, std::move(storage)}};
  return {make_error(ps)};
}

expected<json_value> json_value::parse_file(const std::string& path) {
  return parse_file(path.c_str());
}

// -- free functions -----------------------------------------------------------

std::string to_string(const json_value& val) {
  std::string result;
  val.print_to(result);
  return result;
}

} // namespace caf
