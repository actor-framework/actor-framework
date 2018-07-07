/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <ios>
#include <vector>

#include "caf/sec.hpp"
#include "caf/optional.hpp"
#include "caf/ref_counted.hpp"

#include "caf/detail/raw_ptr.hpp"

namespace caf {
namespace opencl {

// Tag to separate mem_refs from other types in messages.
struct ref_tag {};

class device;

/// A reference type for buffers on a OpenCL devive. Access is not thread safe.
/// Hence, a mem_ref should only be passed to actors sequentially.
template <class T>
class mem_ref : ref_tag {
public:
  using value_type = T;

  friend struct msg_adding_event;
  template <bool PassConfig, class... Ts>
  friend class actor_facade;
  friend class device;

  expected<std::vector<T>> data(optional<size_t> result_size = none) {
    if (!memory_)
      return make_error(sec::runtime_error, "No memory assigned.");
    if (0 != (access_ & CL_MEM_HOST_NO_ACCESS))
      return make_error(sec::runtime_error, "No memory access.");
    if (result_size && *result_size > num_elements_)
      return make_error(sec::runtime_error, "Buffer has less elements.");
    auto num_elements = (result_size ? *result_size : num_elements_);
    auto buffer_size = sizeof(T) * num_elements;
    std::vector<T> buffer(num_elements);
    std::vector<cl_event> prev_events;
    if (event_)
      prev_events.push_back(event_.get());
    cl_event event;
    auto err = clEnqueueReadBuffer(queue_.get(), memory_.get(), CL_TRUE,
                                   0, buffer_size, buffer.data(),
                                   static_cast<cl_uint>(prev_events.size()),
                                   prev_events.data(), &event);
    if (err != CL_SUCCESS)
      return make_error(sec::runtime_error, opencl_error(err));
    // decrements the previous event we used for waiting above
    event_.reset(event, false);
    return buffer;
  }

  void reset() {
    num_elements_ = 0;
    access_ = CL_MEM_HOST_NO_ACCESS;
    memory_.reset();
    access_ = 0;
    event_.reset();
  }

  inline const detail::raw_mem_ptr& get() const {
    return memory_;
  }

  inline size_t size() const {
    return num_elements_;
  }

  inline cl_mem_flags access() const {
    return access_;
  }

  mem_ref()
    : num_elements_{0},
      access_{CL_MEM_HOST_NO_ACCESS},
      queue_{nullptr},
      event_{nullptr},
      memory_{nullptr} {
    // nop
  }

  mem_ref(size_t num_elements, detail::raw_command_queue_ptr queue,
          detail::raw_mem_ptr memory, cl_mem_flags access,
          detail::raw_event_ptr event)
    : num_elements_{num_elements},
      access_{access},
      queue_{queue},
      event_{event},
      memory_{memory} {
    // nop
  }

  mem_ref(size_t num_elements, detail::raw_command_queue_ptr queue,
          cl_mem memory, cl_mem_flags access, detail::raw_event_ptr event)
    : num_elements_{num_elements},
      access_{access},
      queue_{queue},
      event_{event},
      memory_{memory} {
    // nop
  }

  mem_ref(mem_ref&& other) = default;
  mem_ref(const mem_ref& other) = default;
  mem_ref& operator=(mem_ref<T>&& other) = default;
  mem_ref& operator=(const mem_ref& other) = default;

  ~mem_ref() {
    // nop
  }

private:
  inline void set_event(cl_event e, bool increment_reference = true) {
    event_.reset(e, increment_reference);
  }

  inline void set_event(detail::raw_event_ptr e) {
    event_ = std::move(e);
  }

  inline detail::raw_event_ptr event() {
    return event_;
  }

  inline cl_event take_event() {
    return event_.release();
  }

  size_t num_elements_;
  cl_mem_flags access_;
  detail::raw_command_queue_ptr queue_;
  detail::raw_event_ptr event_;
  detail::raw_mem_ptr memory_;
};

/// Updates the reference types in a message with a given event.
struct msg_adding_event {
  msg_adding_event(detail::raw_event_ptr event) : event_(event) {
    // nop
  }
  template <class T, class... Ts>
  message operator()(T& x, Ts&... xs) {
    return make_message(add_event(std::move(x)), add_event(std::move(xs))...);
  }
  template <class... Ts>
  message operator()(std::tuple<Ts...>& values) {
    return apply_args(*this, detail::get_indices(values), values);
  }
  template <class T>
  mem_ref<T> add_event(mem_ref<T> ref) {
    ref.set_event(event_);
    return std::move(ref);
  }
  detail::raw_event_ptr event_;
};

} // namespace opencl

template <class T>
struct allowed_unsafe_message_type<opencl::mem_ref<T>> : std::true_type {};
  
} // namespace caf
