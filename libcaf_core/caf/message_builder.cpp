// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/message_builder.hpp"

#include "caf/detail/meta_object.hpp"
#include "caf/raise_error.hpp"

namespace caf {

namespace {

using namespace detail;

enum to_msg_policy {
  move_msg,
  copy_msg,
};

template <to_msg_policy Policy, class TypeListBuilder, class ElementVector>
message to_message_impl(size_t storage_size, TypeListBuilder& types,
                        ElementVector& elements) {
  if (storage_size == 0)
    return message{};
  auto vptr = malloc(sizeof(message_data) + storage_size);
  if (vptr == nullptr)
    CAF_RAISE_ERROR(std::bad_alloc, "bad_alloc");
  message_data* raw_ptr;
  if constexpr (Policy == move_msg)
    raw_ptr = new (vptr) message_data(types.move_to_list());
  else
    raw_ptr = new (vptr) message_data(types.copy_to_list());
  intrusive_cow_ptr<message_data> ptr{raw_ptr, false};
  auto storage = raw_ptr->storage();
  for (auto& element : elements) {
    if constexpr (Policy == move_msg) {
      storage = element->move_init(storage);
    } else {
      storage = element->copy_init(storage);
    }
    raw_ptr->inc_constructed_elements();
  }
  return message{std::move(ptr)};
}

class message_builder_element_adapter final : public message_builder_element {
public:
  message_builder_element_adapter(message src, size_t index)
    : src_(std::move(src)), index_(index) {
    // nop
  }

  std::byte* copy_init(std::byte* storage) const override {
    auto& meta = global_meta_object(src_.type_at(index_));
    meta.copy_construct(storage, src_.data().at(index_));
    return storage + meta.padded_size;
  }

  std::byte* move_init(std::byte* storage) override {
    if (src_.cptr()->unique()) {
      auto& meta = global_meta_object(src_.type_at(index_));
      meta.move_construct(storage, src_.data().at(index_));
      return storage + meta.padded_size;
    }
    return copy_init(storage);
  }

private:
  message src_;
  size_t index_;
};

} // namespace

message_builder& message_builder::append_from(const caf::message& msg,
                                              size_t first, size_t n) {
  if (!msg || first >= msg.size())
    return *this;
  auto end = std::min(msg.size(), first + n);
  for (size_t index = first; index < end; ++index) {
    auto tid = msg.type_at(index);
    auto& meta = detail::global_meta_object(tid);
    storage_size_ += meta.padded_size;
    types_.push_back(tid);
    elements_.emplace_back(
      std::make_unique<message_builder_element_adapter>(msg, index));
  }
  return *this;
}

void message_builder::clear() noexcept {
  storage_size_ = 0;
  types_.clear();
  elements_.clear();
}

message message_builder::to_message() const {
  return to_message_impl<copy_msg>(storage_size_, types_, elements_);
}

message message_builder::move_to_message() {
  return to_message_impl<move_msg>(storage_size_, types_, elements_);
}

} // namespace caf
