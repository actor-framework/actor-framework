// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <memory>
#include <optional>

namespace caf::flow::gen {

template <class Container>
struct container_and_iterator {
  explicit container_and_iterator(Container values)
    : values_(std::move(values)), pos_(values_.begin()) {
  }

  std::optional<typename Container::value_type> next() {
    if (done())
      return std::nullopt;
    auto item = *pos_;
    ++pos_;
    return item;
  }

  bool done() const {
    return pos_ == values_.end();
  }

private:
  Container values_;
  typename Container::iterator pos_;
};

/// A generator that emits values from a vector.
template <class Container>
class from_container {
public:
  using output_type = typename Container::value_type;

  explicit from_container(Container&& values)
    : values_(std::make_shared<container_and_iterator<Container>>(
        std::move(values))) {
  }

  from_container() = default;
  from_container(from_container&&) = default;
  from_container(const from_container&) = default;
  from_container& operator=(from_container&&) = default;
  from_container& operator=(const from_container&) = default;

  template <class Step, class... Steps>
  void pull(size_t n, Step& step, Steps&... steps) {
    while (n > 0) {
      auto next = values_->next();
      if (!next)
        break;
      if (!step.on_next(std::move(*next), steps...))
        break;
      --n;
    }
    if (values_->done())
      step.on_complete(steps...);
  }

private:
  std::shared_ptr<container_and_iterator<Container>> values_;
};

} // namespace caf::flow::gen
