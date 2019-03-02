/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/ini_consumer.hpp"

#include "caf/pec.hpp"

namespace caf {
namespace detail {

// -- abstract_ini_consumer ----------------------------------------------------

abstract_ini_consumer::abstract_ini_consumer(abstract_ini_consumer* parent)
    : parent_(parent) {
  // nop
}

abstract_ini_consumer::~abstract_ini_consumer() {
  // nop
}

ini_map_consumer abstract_ini_consumer::begin_map() {
  return {this};
}

ini_list_consumer abstract_ini_consumer::begin_list() {
  return {this};
}

// -- map_consumer -------------------------------------------------------------

ini_map_consumer::ini_map_consumer(abstract_ini_consumer* parent)
    : super(parent),
      i_(xs_.end()) {
  CAF_ASSERT(parent != nullptr);
}

ini_map_consumer::ini_map_consumer(ini_map_consumer&& other)
    : super(other.parent()) {
}

ini_map_consumer::~ini_map_consumer() {
  // nop
}

void ini_map_consumer::end_map() {
  parent_->value_impl(config_value{std::move(xs_)});
}

void ini_map_consumer::key(std::string name) {
  i_ = xs_.emplace(std::move(name), config_value{}).first;
}

void ini_map_consumer::value_impl(config_value&& x) {
  CAF_ASSERT(i_ != xs_.end());
  i_->second = std::move(x);
}

// -- ini_list_consumer --------------------------------------------------------

ini_list_consumer::ini_list_consumer(abstract_ini_consumer* parent)
    : super(parent) {
  CAF_ASSERT(parent != nullptr);
}

ini_list_consumer::ini_list_consumer(ini_list_consumer&& other)
    : super(other.parent()),
      xs_(std::move(other.xs_)) {
  // nop
}

void ini_list_consumer::end_list() {
  parent_->value_impl(config_value{std::move(xs_)});
}

void ini_list_consumer::value_impl(config_value&& x) {
  xs_.emplace_back(std::move(x));
}

// -- ini_value_consumer -------------------------------------------------------

ini_value_consumer::ini_value_consumer(abstract_ini_consumer* parent)
    : super(parent) {
  // nop
}

void ini_value_consumer::value_impl(config_value&& x) {
  result = std::move(x);
}

// -- ini_category_consumer ----------------------------------------------------

ini_category_consumer::ini_category_consumer(ini_consumer* parent,
                                             std::string category)
    : super(parent),
      category_(std::move(category)) {
  CAF_ASSERT(parent_ != nullptr);
}

ini_category_consumer::ini_category_consumer(ini_category_consumer&& other)
    : super(other.parent()),
      category_(std::move(other.category_)),
      xs_(std::move(other.xs_)),
      current_key(std::move(other.current_key)) {
  CAF_ASSERT(parent_ != nullptr);
}

void ini_category_consumer::end_map() {
  parent_->value_impl(config_value{std::move(xs_)});
}

void ini_category_consumer::key(std::string name) {
  current_key = std::move(name);
}

void ini_category_consumer::value_impl(config_value&& x) {
  // See whether there's an config_option associated to this category and key.
  auto opt = dparent()->options_.qualified_name_lookup(category_, current_key);
  if (opt == nullptr) {
    // Simply store in config if no option was found.
    xs_.emplace(std::move(current_key), std::move(x));
  } else {
    // Perform a type check to see whether this is a valid input.
    if (opt->check(x) == none) {
      opt->store(x);
      xs_.emplace(std::move(current_key), std::move(x));
    } else {
      dparent()->warnings_.emplace_back(make_error(pec::type_mismatch));
    }
  }
}

ini_consumer* ini_category_consumer::dparent() {
  // Safe, because our constructor only accepts ini_consumer parents.
  return static_cast<ini_consumer*>(parent());
}

// -- ini_consumer -------------------------------------------------------------

ini_consumer::ini_consumer(config_option_set& options, settings& cfg)
    : options_(options),
      cfg_(cfg),
      current_key_("global") {
  // nop
}

ini_category_consumer ini_consumer::begin_map() {
  return {this, current_key_};
}

void ini_consumer::key(std::string name) {
  current_key_ = std::move(name);
}

void ini_consumer::value_impl(config_value&& x) {
  using dict_type = config_value::dictionary;
  auto dict = get_if<dict_type>(&x);
  if (dict == nullptr) {
    warnings_.emplace_back(make_error(pec::type_mismatch,
                                      "expected a dictionary at top level"));
    return;
  }
  if (current_key_ != "global") {
    auto& dst = cfg_.emplace(current_key_, dict_type{}).first->second;
    if (dict != nullptr && !dict->empty() && holds_alternative<dict_type>(dst)) {
      auto& dst_dict = get<dict_type>(dst);
      // We need to "merge" values into the destination, because it can already
      // contain any number of unrelated entries.
      for (auto& entry : *dict)
        dst_dict.insert_or_assign(entry.first, std::move(entry.second));
    }
  } else {
    std::string prev_key;
    swap(current_key_, prev_key);
    for (auto& entry : *dict) {
      if (holds_alternative<dict_type>(entry.second)) {
        // Recurse into top-level maps.
        current_key_ = std::move(entry.first);
        value_impl(std::move(entry.second));
      } else {
        cfg_.insert_or_assign(entry.first, std::move(entry.second));
      }
    }
    swap(prev_key, current_key_);
  }
}

} // namespace detail
} // namespace caf
