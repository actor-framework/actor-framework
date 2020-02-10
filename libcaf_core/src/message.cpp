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

#include <utility>

#include "caf/actor_system.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/dynamic_message_data.hpp"
#include "caf/detail/meta_object.hpp"
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

type_id_list message::types() const noexcept {
  return vals_ != nullptr ? vals_->types() : make_type_id_list();
}

type_id_t message::type(size_t pos) const noexcept {
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
  // Fetch size.
  uint16_t num_elements = 0;
  if (auto err = source(num_elements))
    return err;
  // Short-circuit empty tuples.
  if (num_elements == 0) {
    vals.reset();
    return {};
  }
  // Fetch the meta information (type list).
  // TODO: use a stack-allocated buffer for short type lists.
  auto meta = detail::global_meta_objects();
  std::vector<type_id_t> meta_info;
  meta_info.resize(num_elements + 1);
  meta_info[0] = num_elements;
  for (size_t index = 1; index < (num_elements + 1); ++index) {
    uint16_t& id = meta_info[index];
    if (auto err = source(id))
      return err;
    if (id >= meta.size() || meta[id].type_name == nullptr) {
      CAF_LOG_ERROR("unknown type ID" << id
                                      << "while deserializing a message of size"
                                      << num_elements);
      return sec::unknown_type;
    }
  }
  // Fetch the content of the message.
  auto result = make_counted<detail::dynamic_message_data>();
  for (size_t index = 1; index < (num_elements + 1); ++index) {
    uint16_t id = meta_info[index];
    std::unique_ptr<type_erased_value> element;
    element.reset(meta[id].make());
    if (auto err = element->load(source))
      return err;
    result->append(std::move(element));
  }
  vals = detail::message_data::cow_ptr{std::move(result)};
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
  // Short-circuit empty tuples.
  if (x.empty()) {
    uint16_t zero = 0;
    return sink(zero);
  }
  // Write type information.
  auto* ids = x.types().data();
  for (size_t index = 0; index < ids[0] + 1; ++index)
    if (auto err = sink(ids[index]))
      return err;
  // Write the payload.
  for (size_t index = 0; index < x.size(); ++index)
    if (auto err = x.save(index, sink))
      return err;
  return {};
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
