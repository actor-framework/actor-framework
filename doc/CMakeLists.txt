add_custom_target(doc)

# -- process .in files ---------------------------------------------------------

configure_file("cmake/Doxyfile.in"
               "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
               @ONLY)

# -- Doxygen setup -------------------------------------------------------------

find_package(Doxygen)

if(NOT DOXYGEN_FOUND)
  message(STATUS "Doxygen not found, skip building API documentation.")
else()
  message(STATUS "Add optional target: doxygen.")
  add_custom_target(doxygen "${DOXYGEN_EXECUTABLE}"
                    "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
                    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                    COMMENT "Generating API documentation with Doxygen"
                    VERBATIM)
  add_dependencies(doc doxygen)
endif()
