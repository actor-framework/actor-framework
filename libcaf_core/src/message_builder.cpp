/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <vector>

#include "caf/message_builder.hpp"
#include "caf/message_handler.hpp"
#include "caf/uniform_type_info.hpp"

#include "caf/detail/message_data.hpp"

namespace caf {

class message_builder::dynamic_msg_data : public detail::message_data {
public:
  dynamic_msg_data() : type_token_(0xFFFFFFFF) {
    // nop
  }

  dynamic_msg_data(const dynamic_msg_data& other)
      : detail::message_data(other),
        type_token_(0xFFFFFFFF) {
    for (auto& e : other.elements_) {
      add_to_type_token(e->ti->type_nr());
      elements_.push_back(e->copy());
    }
  }

  dynamic_msg_data(std::vector<uniform_value>&& data)
      : elements_(std::move(data)),
        type_token_(0xFFFFFFFF) {
    for (auto& e : elements_) {
      add_to_type_token(e->ti->type_nr());
    }
  }

  ~dynamic_msg_data();

  const void* at(size_t pos) const override {
    CAF_ASSERT(pos < size());
    return elements_[pos]->val;
  }

  void* mutable_at(size_t pos) override {
    CAF_ASSERT(pos < size());
    return elements_[pos]->val;
  }

  size_t size() const override {
    return elements_.size();
  }

  cow_ptr copy() const override {
    return make_counted<dynamic_msg_data>(*this);
  }

  bool match_element(size_t pos, uint16_t typenr,
                     const std::type_info* rtti) const override {
    CAF_ASSERT(typenr != 0 || rtti != nullptr);
    auto uti = elements_[pos]->ti;
    if (uti->type_nr() != typenr) {
      return false;
    }
    return typenr != 0 || uti->equal_to(*rtti);
  }

  const char* uniform_name_at(size_t pos) const override {
    return elements_[pos]->ti->name();
  }

  uint16_t type_nr_at(size_t pos) const override {
    return elements_[pos]->ti->type_nr();
  }

  uint32_t type_token() const override {
    return type_token_;
  }

  void append(uniform_value&& what) {
    add_to_type_token(what->ti->type_nr());
    elements_.push_back(std::move(what));
  }

  void add_to_type_token(uint16_t typenr) {
    type_token_ = (type_token_ << 6) | typenr;
  }

  void clear() {
    elements_.clear();
    type_token_ = 0xFFFFFFFF;
  }

  std::vector<uniform_value> elements_;
  uint32_t type_token_;
};

message_builder::dynamic_msg_data::~dynamic_msg_data() {
  // avoid weak-vtables warning
}

message_builder::message_builder() {
  init();
}

message_builder::~message_builder() {
  // nop
}

void message_builder::init() {
  // this should really be done by delegating
  // constructors, but we want to support
  // some compilers without that feature...
  data_ = make_counted<dynamic_msg_data>();
}

void message_builder::clear() {
  data()->clear();
}

size_t message_builder::size() const {
  return data()->elements_.size();
}

bool message_builder::empty() const {
  return size() == 0;
}

message_builder& message_builder::append(uniform_value what) {
  data()->append(std::move(what));
  return *this;
}

message message_builder::to_message() const {
  // this const_cast is safe, because the message is
  // guaranteed to detach its data before modifying it
  detail::message_data::cow_ptr ptr;
  ptr.reset(const_cast<dynamic_msg_data*>(data()));
  return message{std::move(ptr)};
}

message message_builder::move_to_message() {
  message result;
  result.vals().reset(static_cast<dynamic_msg_data*>(data_.release()), false);
  return result;
}

optional<message> message_builder::apply(message_handler handler) {
  // avoid detaching of data_ by moving the data to a message object,
  // calling message::apply and moving the data back
  message::data_ptr ptr;
  ptr.reset(static_cast<dynamic_msg_data*>(data_.release()), false);
  message msg{std::move(ptr)};
  auto res = msg.apply(std::move(handler));
  data_.reset(msg.vals().release(), false);
  return res;
}

message_builder::dynamic_msg_data* message_builder::data() {
  // detach if needed, i.e., assume further non-const
  // operations on data_ can cause race conditions if
  // someone else holds a reference to data_
  if (! data_->unique()) {
    auto tmp = static_cast<dynamic_msg_data*>(data_.get())->copy();
    data_.reset(tmp.release(), false);
  }
  return static_cast<dynamic_msg_data*>(data_.get());
}

const message_builder::dynamic_msg_data* message_builder::data() const {
  return static_cast<const dynamic_msg_data*>(data_.get());
}

} // namespace caf
