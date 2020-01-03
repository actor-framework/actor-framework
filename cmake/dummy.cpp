// Some CMake generators (in particular XCode) choke on library targets that
// only consist of a TARGET_OBJECTS generator expression. Adding this otherwise
// useless file to the library target makes sure the generated project files
// have at least one regular source file.

namespace {

[[maybe_unused]] int dummy;

} // namespace
