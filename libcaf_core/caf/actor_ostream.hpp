// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/deep_to_string.hpp"
#include "caf/detail/actor_local_printer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/format.hpp"
#include "caf/typed_actor_pointer.hpp"

#include <string>
#include <string_view>

namespace caf {

/// Provides support for thread-safe output operations on character streams. The
/// stream operates on a per-actor basis and will print only complete lines or
/// when explicitly forced to flush its buffer. The stream will convert *any*
/// operation to a message received by a printer actor. This actor is a sequence
/// point that ensures output appears never interleaved.
class CAF_CORE_EXPORT actor_ostream {
public:
  using fun_type = actor_ostream& (*) (actor_ostream&);

  actor_ostream(actor_ostream&&) = default;
  actor_ostream(const actor_ostream&) = default;
  actor_ostream& operator=(actor_ostream&&) = default;
  actor_ostream& operator=(const actor_ostream&) = default;

  /// Open redirection file in append mode.
  static constexpr int append = 0x01;

  explicit actor_ostream(local_actor* self);

  explicit actor_ostream(scoped_actor& self);

  template <class... Sigs>
  explicit actor_ostream(const typed_actor_pointer<Sigs...>& ptr)
    : actor_ostream(ptr.internal_ptr()) {
    // nop
  }

  /// Adds a new line to the actor output stream.
  template <class... Args>
  actor_ostream& println(std::string_view fmt, Args&&... args) {
    auto buf = detail::format(fmt, std::forward<Args>(args)...);
    buf.push_back('\n');
    printer_->write(std::move(buf));
    return *this;
  }

  /// Writes `arg` to the buffer allocated for the calling actor.
  actor_ostream& write(std::string arg) {
    printer_->write(std::move(arg));
    return *this;
  }

  /// Flushes the buffer allocated for the calling actor.
  actor_ostream& flush() {
    printer_->flush();
    return *this;
  }

  /// Redirects all further output from `self` to `file_name`.
  [[deprecated]] static void redirect(abstract_actor* self, std::string fn,
                                      int flags = 0);

  /// Redirects all further output from any actor that did not
  /// redirect its output to `fname`.
  [[deprecated]] static void redirect_all(actor_system& sys, std::string fn,
                                          int flags = 0);

  /// Writes `arg` to the buffer allocated for the calling actor.
  actor_ostream& operator<<(const char* arg) {
    printer_->write(arg);
    return *this;
  }

  /// Writes `arg` to the buffer allocated for the calling actor.
  actor_ostream& operator<<(std::string arg) {
    printer_->write(std::move(arg));
    return *this;
  }

  /// Writes `to_string(arg)` to the buffer allocated for the calling actor,
  /// calling either `std::to_string` or `caf::to_string` depending on
  /// the argument.
  template <class T>
  actor_ostream& operator<<(const T& arg) {
    printer_->write(deep_to_string(arg));
    return *this;
  }

  /// Apply `f` to `*this`.
  actor_ostream& operator<<(actor_ostream::fun_type f) {
    return f(*this);
  }

private:
  detail::actor_local_printer_ptr printer_;
};

/// Convenience factory function for creating an actor output stream.
CAF_CORE_EXPORT actor_ostream aout(local_actor* self);

// Convenience factory function for creating an actor output stream.
CAF_CORE_EXPORT actor_ostream aout(scoped_actor& self);

/// Convenience factory function for creating an actor output stream.
template <class... Sigs>
actor_ostream aout(const typed_actor_pointer<Sigs...>& ptr) {
  return actor_ostream{ptr};
}

} // namespace caf

namespace std {
// provide convenience overlaods for aout; implemented in logging.cpp
CAF_CORE_EXPORT caf::actor_ostream& endl(caf::actor_ostream& o);
CAF_CORE_EXPORT caf::actor_ostream& flush(caf::actor_ostream& o);
} // namespace std
