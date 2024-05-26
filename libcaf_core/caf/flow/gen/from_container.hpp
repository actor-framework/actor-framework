// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <memory>

namespace caf::flow::gen {

/// A generator that emits values from a vector.
template <class Container>
class from_container {
public:
  using output_type = typename Container::value_type;

  explicit from_container(Container&& values) {
    values_ = std::make_shared<Container>(std::move(values));
    pos_ = values_->begin();
  }

  from_container() = default;
  from_container(from_container&&) = default;
  from_container(const from_container&) = default;
  from_container& operator=(from_container&&) = default;
  from_container& operator=(const from_container&) = default;

  template <class Step, class... Steps>
  void pull(size_t n, Step& step, Steps&... steps) {
    auto end = values_->end();
    while (pos_ != end && n > 0) {
      if (!step.on_next(*pos_++, steps...))
        return;
      --n;
    }
    if (pos_ == end)
      step.on_complete(steps...);
  }

private:
  std::shared_ptr<Container> values_;
  typename Container::const_iterator pos_;
};

} // namespace caf::flow::gen
