// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/caf_deprecated.hpp"

#define CAF_ADD_DEPRECATED_REQUEST_API                                         \
  template <message_priority P = message_priority::normal, class Rep = int,    \
            class Period = std::ratio<1>, class Handle = actor, class... Args> \
  CAF_DEPRECATED("use the mail API instead")                                   \
  auto request(const Handle& dest, std::chrono::duration<Rep, Period> timeout, \
               Args&&... args) {                                               \
    return this->mail(std::forward<Args>(args)...).request(dest, timeout);     \
  }                                                                            \
  template <class MergePolicy,                                                 \
            message_priority Prio = message_priority::normal, class Rep = int, \
            class Period = std::ratio<1>, class Container, class... Args>      \
  CAF_DEPRECATED("use the mail API instead")                                   \
  auto fan_out_request(const Container& destinations,                          \
                       std::chrono::duration<Rep, Period> timeout,             \
                       Args&&... args) {                                       \
    return this->mail(std::forward<Args>(args)...)                             \
      .fan_out_request(destinations, timeout,                                  \
                       typename MergePolicy::tag_type{});                      \
  }
