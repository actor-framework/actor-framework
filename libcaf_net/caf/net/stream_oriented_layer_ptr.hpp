// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/receive_policy.hpp"

namespace caf::net {

/// Wraps a pointer to a stream-oriented layer with a pointer to its lower
/// layer. Both pointers are then used to implement the interface required for a
/// stream-oriented layer when calling into its upper layer.
template <class Layer, class LowerLayerPtr>
class stream_oriented_layer_ptr {
public:
  class access {
  public:
    access(Layer* layer, LowerLayerPtr down) : lptr_(layer), llptr_(down) {
      // nop
    }

    bool can_send_more() const noexcept {
      return lptr_->can_send_more(llptr_);
    }

    auto handle() const noexcept {
      return lptr_->handle(llptr_);
    }

    void begin_output() {
      lptr_->begin_output(llptr_);
    }

    byte_buffer& output_buffer() {
      return lptr_->output_buffer(llptr_);
    }

    void end_output() {
      lptr_->end_output(llptr_);
    }

    void abort_reason(error reason) {
      return lptr_->abort_reason(llptr_, std::move(reason));
    }

    const error& abort_reason() {
      return lptr_->abort_reason(llptr_);
    }

    void configure_read(receive_policy policy) {
      lptr_->configure_read(llptr_, policy);
    }

    bool stopped() const noexcept {
      return lptr_->stopped(llptr_);
    }

  private:
    Layer* lptr_;
    LowerLayerPtr llptr_;
  };

  stream_oriented_layer_ptr(Layer* layer, LowerLayerPtr down)
    : access_(layer, down) {
    // nop
  }

  stream_oriented_layer_ptr(const stream_oriented_layer_ptr&) = default;

  explicit operator bool() const noexcept {
    return true;
  }

  access* operator->() const noexcept {
    return &access_;
  }

  access& operator*() const noexcept {
    return access_;
  }

private:
  mutable access access_;
};

template <class Layer, class LowerLayerPtr>
auto make_stream_oriented_layer_ptr(Layer* this_layer, LowerLayerPtr down) {
  return stream_oriented_layer_ptr<Layer, LowerLayerPtr>{this_layer, down};
}

} // namespace caf::net
