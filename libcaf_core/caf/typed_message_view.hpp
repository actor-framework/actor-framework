// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/message_data.hpp"
#include "caf/detail/offset_at.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/message.hpp"

namespace caf {

template <class... Ts>
class typed_message_view {
public:
  typed_message_view() noexcept : ptr_(nullptr) {
    // nop
  }

  explicit typed_message_view(message& msg)
    : ptr_(msg.types() == make_type_id_list<Ts...>() ? msg.ptr() : nullptr) {
    // nop
  }

  typed_message_view(const typed_message_view&) noexcept = default;

  typed_message_view& operator=(const typed_message_view&) noexcept = default;

  detail::message_data* operator->() noexcept {
    return ptr_;
  }

  explicit operator bool() const noexcept {
    return ptr_ != nullptr;
  }

private:
  detail::message_data* ptr_;
};

template <size_t Index, class... Ts>
auto& get(typed_message_view<Ts...> x) {
  static_assert(Index < sizeof...(Ts));
  using type = caf::detail::tl_at_t<caf::detail::type_list<Ts...>, Index>;
  return *reinterpret_cast<type*>(x->storage()
                                  + detail::offset_at<Index, Ts...>);
}

template <class... Ts>
auto make_typed_message_view(message& msg) {
  return typed_message_view<Ts...>{msg};
}

} // namespace caf
