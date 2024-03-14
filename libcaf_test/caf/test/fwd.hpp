// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <memory>

namespace caf::test {

enum class block_type;
enum class binary_predicate;
enum class unary_predicate;

class and_given;
class and_then;
class and_when;
class block;
class but;
class context;
class factory;
class given;
class nesting_error;
class outline;
class registry;
class reporter;
class runnable;
class runnable_with_examples;
class runner;
class scenario;
class scope;
class section;
class test;
class then;
class when;

using context_ptr = std::shared_ptr<context>;

} // namespace caf::test
