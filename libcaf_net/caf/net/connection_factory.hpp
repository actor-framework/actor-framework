// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"

#include <memory>

namespace caf::net {

/// Creates new socket managers for a @ref socket_acceptor.
template <class Socket>
class connection_factory {
public:
  using socket_type = Socket;

  virtual ~connection_factory() {
    // nop
  }

  virtual error start(net::socket_manager*, const settings&) {
    return none;
  }

  virtual void abort(const error&) {
    // nop
  }

  virtual socket_manager_ptr make(multiplexer*, Socket) = 0;

  template <class Factory>
  static std::unique_ptr<connection_factory>
  decorate(std::unique_ptr<Factory> ptr);
};

template <class Socket>
using connection_factory_ptr = std::unique_ptr<connection_factory<Socket>>;

template <class Socket, class DecoratedSocket>
class connection_factory_decorator : public connection_factory<Socket> {
public:
  using decorated_ptr = connection_factory_ptr<DecoratedSocket>;

  explicit connection_factory_decorator(decorated_ptr decorated)
    : decorated_(std::move(decorated)) {
    // nop
  }

  error start(net::socket_manager* mgr, const settings& cfg) override {
    return decorated_->start(mgr, cfg);
  }

  void abort(const error& reason) override {
    decorated_->abort(reason);
  }

  socket_manager_ptr make(multiplexer* mpx, Socket fd) override {
    return decorated_->make(mpx, fd);
  }

private:
  decorated_ptr decorated_;
};

/// Lifts a factory from a subtype of `Socket` to a factory for `Socket`.
template <class Socket>
template <class Factory>
std::unique_ptr<connection_factory<Socket>>
connection_factory<Socket>::decorate(std::unique_ptr<Factory> ptr) {
  using socket_sub_type = typename Factory::socket_type;
  if constexpr (std::is_same_v<Socket, socket_sub_type>) {
    return ptr;
  } else {
    static_assert(std::is_assignable_v<socket_sub_type, Socket>);
    using decorator_t = connection_factory_decorator<Socket, socket_sub_type>;
    return std::make_unique<decorator_t>(std::move(ptr));
  }
}

} // namespace caf::net
