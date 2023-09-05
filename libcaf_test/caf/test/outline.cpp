// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/outline.hpp"

#include "caf/string_algorithms.hpp"

namespace {

void trim_all(std::vector<std::string_view>& elements) {
  for (auto& element : elements)
    element = caf::trim(element);
}

bool has_duplicates(const std::vector<std::string_view>& elements) {
  std::set<std::string_view> tmp;
  for (auto& element : elements)
    if (!tmp.insert(element).second)
      return true;
  return false;
}

void remove_empty_entries(std::vector<std::string_view>& elements) {
  elements.erase(std::remove_if(elements.begin(), elements.end(),
                                [](const auto& x) { return x.empty(); }),
                 elements.end());
}

} // namespace

namespace caf::test {

outline::examples_setter&
outline::examples_setter::operator=(std::string_view str) {
  if (!examples_)
    return *this;
  // Split up the string into lines.
  std::vector<std::string_view> lines;
  split(lines, str, '\n', token_compress_on);
  trim_all(lines);
  remove_empty_entries(lines);
  if (lines.size() < 2)
    CAF_RAISE_ERROR(std::logic_error, "invalid examples table");
  // Make sure each line is a Markdown-style table row.
  auto is_valid_line = [](std::string_view line) {
    return line.size() > 2 && line.front() == '|' && line.back() == '|';
  };
  if (!std::all_of(lines.begin(), lines.end(), is_valid_line))
    CAF_RAISE_ERROR(std::logic_error, "invalid examples table: syntax error");
  // Strip the leading and trailing pipes.
  for (auto& line : lines)
    line = trim(line.substr(1, line.size() - 2));
  // Split up the first line into column names.
  auto is_non_empty = [](std::string_view str) { return !str.empty(); };
  std::vector<std::string_view> names;
  split(names, lines.front(), '|');
  trim_all(names);
  if (!std::all_of(names.begin(), names.end(), is_non_empty))
    CAF_RAISE_ERROR(std::logic_error,
                    "invalid examples table: empty column names");
  if (has_duplicates(names))
    CAF_RAISE_ERROR(std::logic_error,
                    "invalid examples table: duplicate column names");
  // Parse the remaining lines.
  for (size_t i = 1; i < lines.size(); ++i) {
    std::vector<std::string_view> values;
    split(values, lines[i], '|');
    trim_all(values);
    if (values.size() != names.size())
      CAF_RAISE_ERROR(std::logic_error,
                      "invalid examples table: wrong number of columns");
    std::map<std::string, std::string> row;
    for (size_t j = 0; j < names.size(); ++j)
      row.emplace(names[j], values[j]);
    examples_->emplace_back(std::move(row));
  }
  return *this;
}

void outline::run() {
  if (ctx_->example_parameters.empty()) {
    if (auto guard = ctx_->get<scenario>(-1, description_, loc_)->commit()) {
      // By placing a dummy scenario on the unwind stack, we render all blocks
      // inactive. We are only interested in the assignment to
      // example_parameters.
      scenario dummy{ctx_.get(), -2, description_, loc_};
      ctx_->unwind_stack.push_back(&dummy);
      call_do_run();
      if (ctx_->example_parameters.empty())
        CAF_RAISE_ERROR(std::logic_error,
                        "failed to run outline: no examples found");
    } else {
      CAF_RAISE_ERROR(std::logic_error,
                      "failed to select the root block for the outline");
    }
    ctx_->unwind_stack.clear();
    // Create all descriptions for the examples.
    for (size_t index = 0; index < ctx_->example_parameters.size(); ++index)
      ctx_->example_names.emplace_back(
        detail::format("{} #{}", description_, index + 1));
    // Create all root steps ahead of time.
    for (size_t index = 0; index < ctx_->example_parameters.size(); ++index) {
      auto& ptr = ctx_->steps[std::make_pair(0, index)];
      ptr = std::make_unique<scenario>(ctx_.get(), 0,
                                       ctx_->example_names[index], loc_);
    }
  }
  ctx_->parameters = ctx_->example_parameters[ctx_->example_id];
  auto guard = detail::make_scope_guard([this] {
    auto& ptr = ctx_->steps[std::make_pair(0, ctx_->example_id)];
    if (!ptr->can_run()
        && ctx_->example_id + 1 < ctx_->example_parameters.size())
      ++ctx_->example_id;
  });
  super::run();
}

} // namespace caf::test
