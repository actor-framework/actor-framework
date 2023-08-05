#include "caf/test/bdd_dsl.hpp"

#include <cstdint>

CAF_BEGIN_TYPE_ID_BLOCK(openssl_test, caf::first_custom_type_id)

  CAF_ADD_TYPE_ID(openssl_test, (std::vector<int32_t>) )

CAF_END_TYPE_ID_BLOCK(openssl_test)
