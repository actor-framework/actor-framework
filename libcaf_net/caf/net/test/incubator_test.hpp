#include "caf/fwd.hpp"
#include "caf/test/dsl.hpp"
#include "caf/type_id.hpp"
#include "caf/typed_actor.hpp"

#include <cstdint>
#include <numeric>
#include <string>
#include <utility>

// -- forward declarations for all unit test suites ----------------------------

// -- type IDs for for all unit test suites ------------------------------------

#define ADD_TYPE_ID(type) CAF_ADD_TYPE_ID(incubator_test, type)
#define ADD_ATOM(atom_name) CAF_ADD_ATOM(incubator_test, atom_name)

CAF_BEGIN_TYPE_ID_BLOCK(incubator_test, caf::first_custom_type_id)

CAF_END_TYPE_ID_BLOCK(incubator_test)

#undef ADD_TYPE_ID
#undef ADD_ATOM
