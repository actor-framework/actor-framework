// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/error.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/config.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/message.hpp"
#include "caf/serializer.hpp"

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

error::error(none_t) noexcept : data_(nullptr) {
  // nop
}

error::error(const error& x) : data_(x ? new data(*x.data_) : nullptr) {
  // nop
}

error& error::operator=(const error& x) {
  if (this == &x) {
    // nop
  } else if (x) {
    if (data_ == nullptr)
      data_.reset(new data(*x.data_));
    else
      *data_ = *x.data_;
  } else {
    data_.reset();
  }
  return *this;
}

error::error(uint8_t code, type_id_t category)
  : error(code, category, message{}) {
  // nop
}

error::error(uint8_t code, type_id_t category, message context)
  : data_(code != 0 ? new data{code, category, std::move(context)} : nullptr) {
  // nop
}

// -- observers ----------------------------------------------------------------

int error::compare(const error& x) const noexcept {
  return x ? compare(x.data_->code, x.data_->category) : compare(0, 0);
}

int error::compare(uint8_t code, type_id_t category) const noexcept {
  int x = 0;
  if (data_ != nullptr)
    x = (data_->code << 16) | data_->category;
  return x - int{(code << 16) | category};
}

// -- inspection support -----------------------------------------------------

std::string to_string(const error& x) {
  if (!x)
    return "none";
  std::string result;
  auto code = x.code();
  auto mobj = detail::global_meta_object(x.category());
  mobj->stringify(result, &code);
  auto ctx = x.context();
  if (ctx)
    result += to_string(ctx);
  return result;
}

} // namespace caf
