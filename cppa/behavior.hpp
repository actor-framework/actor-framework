#ifndef BEHAVIOR_HPP
#define BEHAVIOR_HPP

#include "cppa/util/either.hpp"
#include "cppa/invoke_rules.hpp"

namespace cppa {

typedef util::either<invoke_rules, timed_invoke_rules> behavior;

} // namespace cppa

#endif // BEHAVIOR_HPP
