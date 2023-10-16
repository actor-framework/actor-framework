// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_system.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/flow/string.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <cstdio>

namespace caf::detail {

/// A generator that emits characters from a file.
class file_reader {
public:
  using output_type = char;

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
    path_ = other.path_;
    if (file_ != nullptr) {
      fclose(file_);
      file_ = nullptr;
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
      file_ = fopen(path_.c_str(), "r");
      if (!file_) {
        step.on_error(make_error(sec::cannot_open_file), steps...);
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
      if (!step.on_next(static_cast<char>(ch), steps...))
        return;
    }
  }

  void swap(file_reader& other) noexcept {
    using std::swap;
    swap(file_, other.file_);
    swap(path_, other.path_);
  }

private:
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

  template <class F>
  auto run(F&& init) && {
    auto [self, launch] = sys_->spawn_inactive<event_based_actor, detached>();
    auto res = std::forward<F>(init)(gen_(self));
    using res_t = decltype(res);
    static_assert(detail::is_publisher_v<res_t> || detail::is_stream_v<res_t>,
                  "init() must return a publisher or a stream");
    return res;
  }

  auto run() && {
    return std::move(*this).run([](auto&& src) { //
      return std::move(src).to_publisher();
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
        .from_generator(detail::file_reader{std::move(path)});
    };
    return source_runner<decltype(gen)>{sys, std::move(gen)};
  }

  static auto read_lines_impl(actor_system* sys, std::string path) {
    auto gen = [path = std::move(path)](event_based_actor* self) mutable {
      return self //
        ->make_observable()
        .from_generator(detail::file_reader{std::move(path)})
        .transform(flow::string::normalize_newlines())
        .transform(flow::string::to_lines());
    };
    return source_runner<decltype(gen)>{sys, std::move(gen)};
  }

public:
  explicit file(actor_system& sys, std::string path)
    : sys_(&sys), path_(std::move(path)) {
    // nop
  }

  auto read_chars() && {
    return read_chars_impl(sys_, std::move(path_));
  }

  auto read_chars() const& {
    return read_chars_impl(sys_, path_);
  }

  auto read_lines() && {
    return read_lines_impl(sys_, std::move(path_));
  }

  auto read_lines() const& {
    return read_lines_impl(sys_, path_);
  }

private:
  actor_system* sys_;
  std::string path_;
};

} // namespace caf::async
