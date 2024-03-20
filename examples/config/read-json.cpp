// Illustrates how to read custom data types from JSON files.

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/json_reader.hpp"
#include "caf/type_id.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

constexpr std::string_view example_input = R"([
  {
    "id": 1,
    "name": "John Doe"
  },
  {
    "id": 2,
    "name": "Jane Doe",
    "email": "jane@doe.com"
  }
])";

struct user {
  uint32_t id;
  std::string name;
  std::optional<std::string> email;
};

template <class Inspector>
bool inspect(Inspector& f, user& x) {
  return f.object(x).fields(f.field("id", x.id), f.field("name", x.name),
                            f.field("email", x.email));
}

using user_list = std::vector<user>;

CAF_BEGIN_TYPE_ID_BLOCK(example_app, caf::first_custom_type_id)

  CAF_ADD_TYPE_ID(example_app, (user))
  CAF_ADD_TYPE_ID(example_app, (user_list))

CAF_END_TYPE_ID_BLOCK(example_app)

int caf_main(caf::actor_system& sys) {
  // Get file path from config (positional argument).
  auto& cfg = sys.config();
  auto remainder = cfg.remainder();
  if (remainder.size() != 1) {
    std::cerr
      << "*** expected one positional argument: path to a JSON file\n"
      << "\n\nNote: expected a JSON list of user objects. For example:\n"
      << example_input << '\n';
    return EXIT_FAILURE;
  }
  auto& file_path = remainder[0];
  // Read JSON-formatted file.
  caf::json_reader reader;
  if (!reader.load_file(file_path)) {
    std::cerr << "*** failed to parse JSON file: "
              << to_string(reader.get_error()) << '\n';
    return EXIT_FAILURE;
  }
  // Deserialize our user list from the parsed JSON.
  user_list users;
  if (!reader.apply(users)) {
    std::cerr
      << "*** failed to deserialize the user list: "
      << to_string(reader.get_error())
      << "\n\nNote: expected a JSON list of user objects. For example:\n"
      << example_input << '\n';
    return EXIT_FAILURE;
  }
  // Print the list in "CAF format".
  std::cout << "Entries loaded from file:\n";
  for (auto& entry : users)
    std::cout << "- " << caf::deep_to_string(entry) << '\n';
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::id_block::example_app)
