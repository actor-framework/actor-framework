// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_system.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/flow/byte.hpp"
#include "caf/flow/string.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <cstdio>

namespace caf::detail {

/// A generator that emits characters or bytes from a file.
template <class T>
class file_reader {
public:
  static_assert(detail::is_one_of_v<T, std::byte, char>,
                "T must be either std::byte or char");

  using output_type = T;

  explicit file_reader(std::string path) : path_(std::move(path)) {
    // nop
  }

  file_reader(file_reader&& other) noexcept {
    swap(other);
  }

  file_reader& operator=(file_reader&& other) noexcept {
    if (this != &other)
      swap(other);
    return *this;
  }

  file_reader(const file_reader& other) : path_(other.path_) {
    // Note: intentionally don't copy the file handle.
  }

  file_reader& operator=(const file_reader& other) {
    if (this != &other) {
      path_ = other.path_;
      if (file_ != nullptr) {
        fclose(file_);
        file_ = nullptr;
      }
    }
    return *this;
  }

  ~file_reader() {
    if (file_ != nullptr)
      fclose(file_);
  }

  template <class Step, class... Steps>
  void pull(size_t n, Step& step, Steps&... steps) {
    if (file_ == nullptr) {
      file_ = fopen(path_.c_str(), mode());
      if (!file_) {
        step.on_error(error{sec::cannot_open_file}, steps...);
        return;
      }
    }
    for (size_t i = 0; i < n; ++i) {
      auto ch = fgetc(file_);
      if (ch == EOF) {
        step.on_complete(steps...);
        fclose(file_);
        file_ = nullptr;
        return;
      }
      if (!step.on_next(static_cast<output_type>(ch), steps...))
        return;
    }
  }

  void swap(file_reader& other) noexcept {
    using std::swap;
    swap(file_, other.file_);
    swap(path_, other.path_);
  }

private:
  static constexpr const char* mode() noexcept {
    if constexpr (std::is_same_v<output_type, std::byte>)
      return "rb";
    else
      return "r";
  }

  FILE* file_ = nullptr;
  std::string path_;
};

} // namespace caf::detail

namespace caf::async {

/// Helper class to run an asynchronous source.
template <class Factory>
class source_runner {
public:
  source_runner(actor_system* sys, Factory gen)
    : sys_(sys), gen_(std::move(gen)) {
    // nop
  }

  /// Runs the source in a background thread and returns a publisher or stream.
  /// @param init A function object that takes the source as argument and
  ///             turns it into a publisher or stream.
  template <class F>
  auto run(F&& init) && {
    auto [self, launch] = sys_->spawn_inactive<event_based_actor, detached>();
    auto res = std::forward<F>(init)(gen_(self));
    using res_t = decltype(res);
    static_assert(detail::is_publisher_v<res_t> || detail::is_stream_v<res_t>,
                  "init() must return a publisher or a stream");
    return res;
  }

  /// Runs the source in a background thread and returns a publisher or stream.
  auto run() && {
    return std::move(*this).run([](auto&& src) { //
      return std::forward<decltype(src)>(src).to_publisher();
    });
  }

private:
  actor_system* sys_;
  Factory gen_;
};

/// Bundles factories for asynchronous sources and sinks that operate on files.
/// @note Starts a new thread for each source or sink.
class file {
private: // must come before the public interface for return type deduction
  static auto read_chars_impl(actor_system* sys, std::string path) {
    auto gen = [path = std::move(path)](event_based_actor* self) mutable {
      return self //
        ->make_observable()
        .from_generator(detail::file_reader<char>{std::move(path)});
    };
    return source_runner<decltype(gen)>{sys, std::move(gen)};
  }

  static auto read_lines_impl(actor_system* sys, std::string path) {
    auto gen = [path = std::move(path)](event_based_actor* self) mutable {
      return self //
        ->make_observable()
        .from_generator(detail::file_reader<char>{std::move(path)})
        .transform(flow::string::normalize_newlines())
        .transform(flow::string::to_lines());
    };
    return source_runner<decltype(gen)>{sys, std::move(gen)};
  }

  static auto read_bytes_impl(actor_system* sys, std::string path) {
    auto gen = [path = std::move(path)](event_based_actor* self) mutable {
      return self //
        ->make_observable()
        .from_generator(detail::file_reader<std::byte>{std::move(path)});
    };
    return source_runner<decltype(gen)>{sys, std::move(gen)};
  }

  static auto read_chunks_impl(actor_system* sys, std::string path, size_t n) {
    auto gen = [path = std::move(path), n](event_based_actor* self) mutable {
      return self //
        ->make_observable()
        .from_generator(detail::file_reader<std::byte>{std::move(path)})
        .transform(flow::byte::to_chunks(n));
    };
    return source_runner<decltype(gen)>{sys, std::move(gen)};
  }

public:
  file(actor_system& sys, std::string path)
    : sys_(&sys), path_(std::move(path)) {
    // nop
  }

  /// Asynchronously reads the entire file, character by character.
  [[nodiscard]] auto read_chars() && {
    return read_chars_impl(sys_, std::move(path_));
  }

  /// Asynchronously reads the entire file, character by character.
  [[nodiscard]] auto read_chars() const& {
    return read_chars_impl(sys_, path_);
  }

  /// Asynchronously reads the entire file, line by line.
  [[nodiscard]] auto read_lines() && {
    return read_lines_impl(sys_, std::move(path_));
  }

  /// Asynchronously reads the entire file, line by line.
  [[nodiscard]] auto read_lines() const& {
    return read_lines_impl(sys_, path_);
  }

  /// Asynchronously reads the entire file, byte by byte.
  [[nodiscard]] auto read_bytes() const& {
    return read_bytes_impl(sys_, path_);
  }

  /// Asynchronously reads the entire file, byte by byte.
  [[nodiscard]] auto read_bytes() && {
    return read_bytes_impl(sys_, std::move(path_));
  }

  /// Asynchronously reads the entire file, grouped into chunks of size
  /// `chunk_size`.
  [[nodiscard]] auto read_chunks(size_t chunk_size) const& {
    return read_chunks_impl(sys_, path_, chunk_size);
  }

  /// Asynchronously reads the entire file, grouped into chunks of size
  /// `chunk_size`.
  [[nodiscard]] auto read_chunks(size_t chunk_size) && {
    return read_chunks_impl(sys_, std::move(path_), chunk_size);
  }

private:
  actor_system* sys_;
  std::string path_;
};

} // namespace caf::async
