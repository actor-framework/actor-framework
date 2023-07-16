// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/config_consumer.hpp"

#include "caf/detail/overload.hpp"
#include "caf/pec.hpp"

namespace caf::detail {

// -- config_list_consumer -----------------------------------------------------

config_list_consumer::config_list_consumer(const config_option_set* options,
                                           config_consumer* parent)
  : options_(options), parent_(parent) {
  // nop
}

config_list_consumer::config_list_consumer(const config_option_set* options,
                                           config_list_consumer* parent)
  : options_(options), parent_(parent) {
  // nop
}

config_list_consumer::config_list_consumer(config_value_consumer* parent)
  : parent_(parent) {
  // nop
}

pec config_list_consumer::end_list() {
  auto f = make_overload([](none_t) { return pec::success; },
                         [this](config_consumer* ptr) {
                           return ptr->value(config_value{std::move(result)});
                         },
                         [this](auto* ptr) {
                           ptr->value(config_value{std::move(result)});
                           return pec::success;
                         });
  return visit(f, parent_);
}

config_consumer config_list_consumer::begin_map() {
  return config_consumer{options_, this};
}

std::string config_list_consumer::qualified_key() {
  auto f = make_overload([](none_t) { return std::string{}; },
                         [](config_value_consumer*) { return std::string{}; },
                         [](auto* ptr) { return ptr->qualified_key(); });
  return visit(f, parent_);
}

// -- config_consumer ----------------------------------------------------------

config_consumer::config_consumer(const config_option_set* options,
                                 config_consumer* parent)
  : options_(options),
    parent_(parent),
    cfg_(new settings),
    category_(parent->qualified_key()) {
  // nop
}

config_consumer::config_consumer(const config_option_set* options,
                                 config_list_consumer* parent)
  : options_(options),
    parent_(parent),
    cfg_(new settings),
    category_(parent->qualified_key()) {
  // nop
}

config_consumer::config_consumer(const config_option_set& options,
                                 settings& cfg)
  : options_(&options), cfg_(&cfg), category_("global") {
  // nop
}

config_consumer::config_consumer(settings& cfg) : cfg_(&cfg) {
  // nop
}

config_consumer::config_consumer(config_consumer&& other)
  : options_(other.options_), parent_(other.parent_), cfg_(other.cfg_) {
  other.parent_ = none;
}

config_consumer& config_consumer::operator=(config_consumer&& other) {
  destroy();
  options_ = other.options_;
  parent_ = other.parent_;
  cfg_ = other.cfg_;
  other.parent_ = none;
  return *this;
}

config_consumer::~config_consumer() {
  destroy();
}

void config_consumer::destroy() {
  if (!holds_alternative<none_t>(parent_))
    delete cfg_;
}

void config_consumer::end_map() {
  auto f = [this](auto ptr) {
    if constexpr (std::is_same<none_t, decltype(ptr)>::value) {
      // nop
    } else {
      ptr->value(std::move(*cfg_));
    }
  };
  visit(f, parent_);
}

std::string config_consumer::qualified_key() {
  if (category_.empty() || category_ == "global")
    return current_key_;
  auto result = category_;
  result += '.';
  result += current_key_;
  return result;
}

namespace {

void merge_into_place(settings& src, settings& dst) {
  for (auto& [key, value] : src) {
    if (auto src_sub = get_if<config_value::dictionary>(&value)) {
      auto i = dst.find(key);
      if (i == dst.end() || !holds_alternative<settings>(i->second))
        dst.insert_or_assign(key, std::move(value));
      else
        merge_into_place(*src_sub, get<settings>(i->second));
    } else {
      dst.insert_or_assign(key, std::move(value));
    }
  }
}

} // namespace

pec config_consumer::value_impl(config_value&& x) {
  // Sync with config option object if available.
  if (options_ != nullptr)
    if (auto opt = options_->qualified_name_lookup(category_, current_key_))
      if (auto err = opt->sync(x))
        return pec::type_mismatch;
  // Insert / replace value in the map.
  if (auto dict = get_if<settings>(&x)) {
    // Merge values into the destination, because it can already contain any
    // number of unrelated entries.
    auto i = cfg_->find(current_key_);
    if (i == cfg_->end() || !holds_alternative<settings>(i->second))
      cfg_->insert_or_assign(current_key_, std::move(x));
    else
      merge_into_place(*dict, get<settings>(i->second));
  } else {
    cfg_->insert_or_assign(current_key_, std::move(x));
  }
  return pec::success;
}

// -- config_value_consumer ----------------------------------------------------

config_list_consumer config_value_consumer::begin_list() {
  return config_list_consumer{this};
}

config_consumer config_value_consumer::begin_map() {
  return config_consumer{result.as_dictionary()};
}

} // namespace caf::detail
