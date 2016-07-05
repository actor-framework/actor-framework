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

#ifndef CAF_MAILBOX_ELEMENT_HPP
#define CAF_MAILBOX_ELEMENT_HPP

#include <cstddef>

#include "caf/extend.hpp"
#include "caf/message.hpp"
#include "caf/message_id.hpp"
#include "caf/ref_counted.hpp"
#include "caf/make_message.hpp"
#include "caf/message_view.hpp"
#include "caf/memory_managed.hpp"
#include "caf/type_erased_tuple.hpp"
#include "caf/actor_control_block.hpp"

#include "caf/meta/type_name.hpp"
#include "caf/meta/omittable_if_empty.hpp"

#include "caf/detail/disposer.hpp"
#include "caf/detail/tuple_vals.hpp"
#include "caf/detail/type_erased_tuple_view.hpp"

namespace caf {

class mailbox_element : public memory_managed, public message_view {
public:
  using forwarding_stack = std::vector<strong_actor_ptr>;

  /// Intrusive pointer to the next mailbox element.
  mailbox_element* next;

  /// Intrusive pointer to the previous mailbox element.
  mailbox_element* prev;

  /// Avoids multi-processing in blocking actors via flagging.
  bool marked;

  /// Source of this message and receiver of the final response.
  strong_actor_ptr sender;

  /// Denotes whether this an asynchronous message or a request.
  message_id mid;

  /// `stages.back()` is the next actor in the forwarding chain,
  /// if this is empty then the original sender receives the response.
  forwarding_stack stages;

  mailbox_element();

  mailbox_element(strong_actor_ptr&& sender, message_id id,
                  forwarding_stack&& stages);

  virtual ~mailbox_element();

  type_erased_tuple& content() override;

  message move_content_to_message() override;

  const type_erased_tuple& content() const;

  mailbox_element(mailbox_element&&) = delete;
  mailbox_element(const mailbox_element&) = delete;
  mailbox_element& operator=(mailbox_element&&) = delete;
  mailbox_element& operator=(const mailbox_element&) = delete;

  inline bool is_high_priority() const {
    return mid.is_high_priority();
  }

protected:
  empty_type_erased_tuple dummy_;
};

/// @relates mailbox_element
template <class Inspector>
void inspect(Inspector& f, mailbox_element& x) {
  return f(meta::type_name("mailbox_element"), x.sender, x.mid,
           meta::omittable_if_empty(), x.stages, x.content());
}

/// Encapsulates arbitrary data in a message element.
template <class... Ts>
class mailbox_element_vals
    : public mailbox_element,
      public detail::tuple_vals_impl<type_erased_tuple, Ts...> {
public:
  template <class... Us>
  mailbox_element_vals(strong_actor_ptr&& sender, message_id id,
                       forwarding_stack&& stages, Us&&... xs)
      : mailbox_element(std::move(sender), id, std::move(stages)),
        detail::tuple_vals_impl<type_erased_tuple, Ts...>(std::forward<Us>(xs)...) {
    // nop
  }

  type_erased_tuple& content() {
    return *this;
  }

  message move_content_to_message() {
    message_factory f;
    auto& xs = this->data();
    return detail::apply_moved_args(f, detail::get_indices(xs), xs);
  }

  void dispose() noexcept {
    this->deref();
  }
};

/// Provides a view for treating arbitrary data as message element.
template <class... Ts>
class mailbox_element_view : public mailbox_element,
                             public detail::type_erased_tuple_view<Ts...> {
public:
  mailbox_element_view(strong_actor_ptr&& sender, message_id id,
                       forwarding_stack&& stages, Ts&... xs)
    : mailbox_element(std::move(sender), id, std::move(stages)),
      detail::type_erased_tuple_view<Ts...>(xs...) {
    // nop
  }

  type_erased_tuple& content() override {
    return *this;
  }

  message move_content_to_message() override {
    message_factory f;
    auto& xs = this->data();
    return detail::apply_moved_args(f, detail::get_indices(xs), xs);
  }
};

/// @relates mailbox_element
using mailbox_element_ptr = std::unique_ptr<mailbox_element, detail::disposer>;

/// @relates mailbox_element
mailbox_element_ptr
make_mailbox_element(strong_actor_ptr sender, message_id id,
                     mailbox_element::forwarding_stack stages, message msg);

/// @relates mailbox_element
template <class T, class... Ts>
typename std::enable_if<
  ! std::is_same<typename std::decay<T>::type, message>::value
  || (sizeof...(Ts) > 0),
  mailbox_element_ptr
>::type
make_mailbox_element(strong_actor_ptr sender, message_id id,
                     mailbox_element::forwarding_stack stages,
                     T&& x, Ts&&... xs) {
  using impl =
    mailbox_element_vals<
      typename unbox_message_element<
        typename detail::strip_and_convert<T>::type
      >::type,
      typename unbox_message_element<
        typename detail::strip_and_convert<Ts>::type
      >::type...
    >;
  auto ptr = new impl(std::move(sender), id, std::move(stages),
                      std::forward<T>(x), std::forward<Ts>(xs)...);
  return mailbox_element_ptr{ptr};
}

} // namespace caf

#endif // CAF_MAILBOX_ELEMENT_HPP
