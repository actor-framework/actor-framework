// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#define CAF_CONCAT_(LHS, RHS) LHS ## RHS
#define CAF_CONCAT(LHS, RHS) CAF_CONCAT_(LHS, RHS)
#define CAF_UNIFYN(NAME) CAF_CONCAT(NAME, __LINE__)

