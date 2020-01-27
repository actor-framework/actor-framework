/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/message.hpp"

#include <iostream>
#include <utility>
#include <utility>

#include "caf/actor_system.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/dynamic_message_data.hpp"
#include "caf/message_builder.hpp"
#include "caf/message_handler.hpp"
#include "caf/serializer.hpp"
#include "caf/string_algorithms.hpp"

namespace caf {

message::message(none_t) noexcept {
  // nop
}

message::message(message&& other) noexcept : vals_(std::move(other.vals_)) {
  // nop
}

message::message(data_ptr ptr) noexcept : vals_(std::move(ptr)) {
  // nop
}

message& message::operator=(message&& other) noexcept {
  vals_.swap(other.vals_);
  return *this;
}

message::~message() {
  // nop
}

// -- implementation of type_erased_tuple --------------------------------------

void* message::get_mutable(size_t p) {
  CAF_ASSERT(vals_ != nullptr);
  return vals_.unshared().get_mutable(p);

}

error message::load(size_t pos, deserializer& source) {
  CAF_ASSERT(vals_ != nullptr);
  return vals_.unshared().load(pos, source);
}

error_code<sec> message::load(size_t pos, binary_deserializer& source) {
  CAF_ASSERT(vals_ != nullptr);
  return vals_.unshared().load(pos, source);
}

size_t message::size() const noexcept {
  return vals_ != nullptr ? vals_->size() : 0;
}

rtti_pair message::type(size_t pos) const noexcept {
  CAF_ASSERT(vals_ != nullptr);
  return vals_->type(pos);
}

const void* message::get(size_t pos) const noexcept {
  CAF_ASSERT(vals_ != nullptr);
  return vals_->get(pos);
}

std::string message::stringify(size_t pos) const {
  CAF_ASSERT(vals_ != nullptr);
  return vals_->stringify(pos);
}

type_erased_value_ptr message::copy(size_t pos) const {
  CAF_ASSERT(vals_ != nullptr);
  return vals_->copy(pos);
}

error message::save(size_t pos, serializer& sink) const {
  CAF_ASSERT(vals_ != nullptr);
  return vals_->save(pos, sink);
}

error_code<sec> message::save(size_t pos, binary_serializer& sink) const {
  CAF_ASSERT(vals_ != nullptr);
  return vals_->save(pos, sink);
}

bool message::shared() const noexcept {
  return vals_ != nullptr ? vals_->shared() : false;
}

namespace {

template <class Deserializer>
typename Deserializer::result_type
load_vals(Deserializer& source, message::data_ptr& vals) {
  if (source.context() == nullptr)
    return sec::no_context;
  uint16_t zero = 0;
  std::string tname;
  if (auto err = source.begin_object(zero, tname))
    return err;
  if (zero != 0)
    return sec::unknown_type;
  if (tname == "@<>") {
    vals.reset();
    return none;
  }
  if (tname.compare(0, 4, "@<>+") != 0)
    return sec::unknown_type;
  // iterate over concatenated type names
  auto eos = tname.end();
  auto next = [&](std::string::iterator iter) {
    return std::find(iter, eos, '+');
  };
  auto& types = source.context()->system().types();
  auto dmd = make_counted<detail::dynamic_message_data>();
  std::string tmp;
  std::string::iterator i = next(tname.begin());
  ++i; // skip first '+' sign
  do {
    auto n = next(i);
    tmp.assign(i, n);
    auto ptr = types.make_value(tmp);
    if (!ptr) {
      CAF_LOG_ERROR("unknown type:" << tmp);
      return sec::unknown_type;
    }
    if (auto err = ptr->load(source))
      return err;
    dmd->append(std::move(ptr));
    if (n != eos)
      i = n + 1;
    else
      i = eos;
  } while (i != eos);
  if (auto err = source.end_object())
    return err;
  vals = detail::message_data::cow_ptr{std::move(dmd)};
  return none;
}

} // namespace

error message::load(deserializer& source) {
  return load_vals(source, vals_);
}

error_code<sec> message::load(binary_deserializer& source) {
  return load_vals(source, vals_);
}

namespace {

template <class Serializer>
typename Serializer::result_type
save_tuple(Serializer& sink, const type_erased_tuple& x) {
  if (sink.context() == nullptr)
    return sec::no_context;
  // build type name
  uint16_t zero = 0;
  std::string tname = "@<>";
  if (x.empty()) {
    if (auto err = sink.begin_object(zero, tname))
      return err;
    return sink.end_object();
  }
  auto& types = sink.context()->system().types();
  auto n = x.size();
  for (size_t i = 0; i < n; ++i) {
    auto rtti = x.type(i);
    const auto& portable_name = types.portable_name(rtti);
    if (portable_name == types.default_type_name()) {
      std::cerr << "[ERROR]: cannot serialize message because a type was "
                   "not added to the types list, typeid name: "
                << (rtti.second != nullptr ? rtti.second->name()
                                           : "-not-available-")
                << std::endl;
      return sec::unknown_type;
    }
    tname += '+';
    tname += portable_name;
  }
  if (auto err = sink.begin_object(zero, tname))
    return err;
  for (size_t i = 0; i < n; ++i)
    if (auto err = x.save(i, sink))
      return err;
  return sink.end_object();
}

} // namespace

error message::save(serializer& sink, const type_erased_tuple& x) {
  return save_tuple(sink, x);
}

error_code<sec>
message::save(binary_serializer& sink, const type_erased_tuple& x) {
  return save_tuple(sink, x);
}

error message::save(serializer& sink) const {
  return save_tuple(sink, *this);
}

error_code<sec> message::save(binary_serializer& sink) const {
  return save_tuple(sink, *this);
}

// -- factories ----------------------------------------------------------------

message message::copy(const type_erased_tuple& xs) {
  message_builder mb;
  for (size_t i = 0; i < xs.size(); ++i)
    mb.emplace(xs.copy(i));
  return mb.move_to_message();
}
// -- modifiers ----------------------------------------------------------------


optional<message> message::apply(message_handler handler) {
  return handler(*this);
}

void message::swap(message& other) noexcept {
  vals_.swap(other.vals_);
}

void message::reset(raw_ptr new_ptr, bool add_ref) noexcept {
  vals_.reset(new_ptr, add_ref);
}

error inspect(serializer& sink, message& msg) {
  return msg.save(sink);
}

error inspect(deserializer& source, message& msg) {
  return msg.load(source);
}

error_code<sec> inspect(binary_serializer& sink, message& msg) {
  return msg.save(sink);
}

error_code<sec> inspect(binary_deserializer& source, message& msg) {
  return msg.load(source);
}

std::string to_string(const message& msg) {
  if (msg.empty())
    return "<empty-message>";
  std::string str = "(";
  str += msg.cvals()->stringify(0);
  for (size_t i = 1; i < msg.size(); ++i) {
    str += ", ";
    str += msg.cvals()->stringify(i);
  }
  str += ")";
  return str;
}

} // namespace caf
