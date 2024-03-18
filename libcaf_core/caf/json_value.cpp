// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/json_value.hpp"

#include "caf/detail/format.hpp"
#include "caf/expected.hpp"
#include "caf/json_array.hpp"
#include "caf/json_object.hpp"
#include "caf/make_counted.hpp"
#include "caf/parser_state.hpp"

#include <cstdint>
#include <fstream>

namespace caf::detail {
namespace {

const auto null_value_instance = json::value{};

const auto undefined_value_instance = json::value{json::undefined_t{}};

} // namespace
} // namespace caf::detail

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

json_value::json_value() noexcept : val_(&detail::null_value_instance) {
  // nop
}

// -- factories ----------------------------------------------------------------

json_value json_value::undefined() noexcept {
  return json_value{&detail::undefined_value_instance, nullptr};
}

// -- properties ---------------------------------------------------------------

bool json_value::is_integer() const noexcept {
  return val_->is_integer()
         || (val_->is_unsigned()
             && std::get<uint64_t>(val_->data) <= INT64_MAX);
}

bool json_value::is_unsigned() const noexcept {
  return val_->is_unsigned()
         || (val_->is_integer() && std::get<int64_t>(val_->data) >= 0);
}

bool json_value::is_number() const noexcept {
  switch (val_->data.index()) {
    default:
      return false;
    case detail::json::value::integer_index:
    case detail::json::value::unsigned_index:
    case detail::json::value::double_index:
      return true;
  }
}

// -- conversion ---------------------------------------------------------------

int64_t json_value::to_integer(int64_t fallback) const {
  switch (val_->data.index()) {
    default:
      return fallback;
    case detail::json::value::integer_index:
      return std::get<int64_t>(val_->data);
    case detail::json::value::unsigned_index: {
      auto val = std::get<uint64_t>(val_->data);
      if (val <= INT64_MAX)
        return static_cast<int64_t>(val);
      return fallback;
    }
    case detail::json::value::double_index:
      return static_cast<int64_t>(std::get<double>(val_->data));
  }
}

uint64_t json_value::to_unsigned(uint64_t fallback) const {
  switch (val_->data.index()) {
    default:
      return fallback;
    case detail::json::value::integer_index: {
      auto val = std::get<int64_t>(val_->data);
      if (val >= 0)
        return static_cast<uint64_t>(val);
      return fallback;
    }
    case detail::json::value::unsigned_index:
      return std::get<uint64_t>(val_->data);
    case detail::json::value::double_index:
      return static_cast<int64_t>(std::get<double>(val_->data));
  }
}

double json_value::to_double(double fallback) const {
  switch (val_->data.index()) {
    default:
      return fallback;
    case detail::json::value::integer_index:
      return static_cast<double>(std::get<int64_t>(val_->data));
    case detail::json::value::unsigned_index:
      return static_cast<double>(std::get<uint64_t>(val_->data));
    case detail::json::value::double_index:
      return std::get<double>(val_->data);
  };
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
  return {ps.error()};
}

expected<json_value> json_value::parse_shallow(std::string_view str) {
  auto storage = make_counted<detail::json::storage>();
  string_parser_state ps{str.begin(), str.end()};
  auto root = detail::json::parse_shallow(ps, &storage->buf);
  if (ps.code == pec::success)
    return {json_value{root, std::move(storage)}};
  return {ps.error()};
}

expected<json_value> json_value::parse_in_situ(std::string& str) {
  auto storage = make_counted<detail::json::storage>();
  detail::json::mutable_string_parser_state ps{str.data(),
                                               str.data() + str.size()};
  auto root = detail::json::parse_in_situ(ps, &storage->buf);
  if (ps.code == pec::success)
    return {json_value{root, std::move(storage)}};
  return {ps.error()};
}

expected<json_value> json_value::parse_file(const char* path) {
  using iterator_t = std::istreambuf_iterator<char>;
  std::ifstream input{path};
  if (!input.is_open())
    return error{sec::cannot_open_file};
  auto storage = make_counted<detail::json::storage>();
  detail::json::file_parser_state ps{iterator_t{input}, iterator_t{}};
  auto root = detail::json::parse(ps, &storage->buf);
  if (ps.code == pec::success)
    return {json_value{root, std::move(storage)}};
  return {ps.error()};
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
