# seeks the beginning of the enum while also keeping track of namespaces
macro(seek_enum_mode)
  if(line MATCHES "^namespace ")
    string(REGEX REPLACE "^namespace ([a-zA-Z0-9:_]+).*$" "\\1" tmp "${line}")
    list(APPEND namespaces "${tmp}")
  elseif(line MATCHES "^enum ")
    if(NOT namespaces)
      message(FATAL_ERROR "enum found outside of a namespace")
    endif()
    if(line MATCHES "^enum class ")
      set(is_enum_class TRUE)
    endif()
    string(REGEX REPLACE "^enum (class )?([0-9a-zA-Z_]+).*$" "\\2" enum_name "${line}")
    set(mode "read_values")
  endif()
endmacro()

# scans the input for enum values
macro(read_values_mode)
  if(line MATCHES "^}")
    set(mode "generate")
  elseif(line MATCHES "^[0-9a-zA-Z_]+")
    string(REGEX REPLACE "^([0-9a-zA-Z_]+).*$" "\\1" tmp "${line}")
    list(APPEND enum_values "${tmp}")
  endif()
endmacro()

macro(write_enum_file)
  if(is_enum_class)
    set(label_prefix "${enum_name}::")
  else()
    set(label_prefix "")
  endif()
  string(REPLACE ";" "::" namespace_str "${namespaces}")
  string(REPLACE "::" "/" namespace_path "${namespace_str}")
  set(out "")
  # File header and includes.
  list(APPEND out
    "#include \"caf/config.hpp\"\n"
    "\n"
    "CAF_PUSH_DEPRECATED_WARNING\n"
    "\n"
    "#include \"${namespace_path}/${enum_name}.hpp\"\n"
    "\n"
    "#include <string>\n"
    "#include <string_view>\n"
    "\n"
    "namespace ${namespace_str} {\n"
    "\n")
  # Generate to_string implementation.
  list(APPEND out
    "std::string to_string(${enum_name} x) {\n"
    "  switch (x) {\n"
    "    default:\n"
    "      return \"???\"\;\n")
  foreach(label IN LISTS enum_values)
    list(APPEND out
      "    case ${label_prefix}${label}:\n"
      "      return \"${label}\"\;\n")
  endforeach()
  list(APPEND out
    "  }\n"
     "}\n"
     "\n")
  # Generate from_string implementation.
  list(APPEND out
    "bool from_string(std::string_view in, ${enum_name}& out) {\n")
  foreach(label IN LISTS enum_values)
    list(APPEND out
      "  if (in == \"${label}\"\n"
      "      || in == \"${namespace_str}::${enum_name}::${label}\") {\n"
      "    out = ${label_prefix}${label}\;\n"
      "    return true\;\n"
      "  }\n")
  endforeach()
  list(APPEND out
    "  return false\;\n"
    "}\n"
    "\n")
  # Generate from_integer implementation.
  list(APPEND out
    "bool from_integer(std::underlying_type_t<${enum_name}> in,\n"
    "                  ${enum_name}& out) {\n"
    "  auto result = static_cast<${enum_name}>(in)\;\n"
    "  switch (result) {\n"
    "    default:\n"
    "      return false\;\n")
  foreach(label IN LISTS enum_values)
    list(APPEND out
      "    case ${label_prefix}${label}:\n")
  endforeach()
  list(APPEND out
    "      out = result\;\n"
    "      return true\;\n"
    "  }\n"
    "}\n"
    "\n")
 # File footer.
 list(APPEND out
   "} // namespace ${namespace_str}\n"
   "\n")
 file(WRITE "${OUTPUT_FILE}" ${out})
endmacro()

function(main)
  set(namespaces "")
  set(enum_name "")
  set(enum_values "")
  set(is_enum_class FALSE)
  file(STRINGS "${INPUT_FILE}" lines)
  set(mode "seek_enum")
  foreach(line IN LISTS lines)
    string(REGEX REPLACE "^(.+)(//.*)?" "\\1" line "${line}")
    string(STRIP "${line}" line)
    if(mode STREQUAL seek_enum)
      seek_enum_mode()
    elseif(mode STREQUAL read_values)
      read_values_mode()
    else()
      # reached the end
      if(NOT enum_name)
        message(FATAL_ERROR "No enum found in input file")
      endif()
      if(NOT enum_values)
        message(FATAL_ERROR "No enum values found for ${enum_name}")
      endif()
      write_enum_file()
      return()
    endif()
  endforeach()
endfunction()

# - check parameters and run ---------------------------------------------------

if(NOT INPUT_FILE)
  message(FATAL_ERROR "Variable INPUT_FILE undefined")
endif()

if(NOT OUTPUT_FILE)
  message(FATAL_ERROR "Variable OUTPUT_FILE undefined")
endif()

main()
