add_custom_target(doc)

# -- list all .tex source files ------------------------------------------------

set(sources
  tex/Actors.tex
  tex/Brokers.tex
  tex/CommonPitfalls.tex
  tex/ConfiguringActorApplications.tex
  tex/Error.tex
  tex/FAQ.tex
  tex/FirstSteps.tex
  tex/GroupCommunication.tex
  tex/Introduction.tex
  tex/ManagingGroupsOfWorkers.tex
  tex/MessageHandlers.tex
  tex/MessagePassing.tex
  tex/Messages.tex
  tex/MigrationGuides.tex
  tex/NetworkTransparency.tex
  tex/OpenCL.tex
  tex/ReferenceCounting.tex
  tex/Registry.tex
  tex/RemoteSpawn.tex
  tex/Scheduler.tex
  tex/Streaming.tex
  tex/TypeInspection.tex
  tex/UsingAout.tex
  tex/Utility.tex
  tex/Testing.tex
)

# -- create target folders -----------------------------------------------------

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tex")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/rst")

# -- process .in files ---------------------------------------------------------

configure_file("cmake/Doxyfile.in"
               "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
               @ONLY)

configure_file("cmake/variables.tex.in"
               "${CMAKE_CURRENT_BINARY_DIR}/tex/variables.tex"
               @ONLY)

configure_file("cmake/conf.py.in"
               "${CMAKE_CURRENT_BINARY_DIR}/rst/conf.py"
               @ONLY)

configure_file("cmake/index_footer.rst.in"
               "${CMAKE_CURRENT_BINARY_DIR}/rst/index_footer.rst"
               @ONLY)

configure_file("cmake/index_header.rst.in"
               "${CMAKE_CURRENT_BINARY_DIR}/rst/index_header.rst"
               @ONLY)

# -- generate .rst files -------------------------------------------------------


add_executable(caf-generate-rst cmake/caf-generate-rst.cpp)
target_link_libraries(caf-generate-rst
                      ${CAF_EXTRA_LDFLAGS}
                      ${CAF_LIBRARIES}
                      ${PTHREAD_LIBRARIES})

add_custom_target(rst)

add_dependencies(doc rst)

function(convert_to_rst tex_file)
  get_filename_component(file_name "${tex_file}" NAME_WE)
  set(input "${CMAKE_CURRENT_SOURCE_DIR}/tex/${tex_file}")
  set(rst_file "${file_name}.rst")
  set(output "${CMAKE_CURRENT_BINARY_DIR}/rst/${rst_file}")
  add_custom_command(OUTPUT "${output}"
                     COMMAND
                       caf-generate-rst
                       -o "${output}"
                       -i "${input}"
                       -r "${PROJECT_SOURCE_DIR}"
                     DEPENDS caf-generate-rst "${input}")
  add_custom_target("${rst_file}" DEPENDS "${output}")
  add_dependencies(rst "${rst_file}")
endfunction()

foreach(filename ${sources})
  get_filename_component(filename_no_dir "${filename}" NAME)
  convert_to_rst("${filename_no_dir}")
endforeach()

# generate index.rst file from manual.tex
add_custom_target("index.rst"
                  DEPENDS "tex/manual.tex"
                  COMMAND "python"
                          "${CMAKE_SOURCE_DIR}/scripts/make_index_rst.py"
                          "${CMAKE_CURRENT_BINARY_DIR}/rst/index.rst"
                          "${CMAKE_SOURCE_DIR}/doc/tex/manual.tex"
                  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/rst")
add_dependencies(rst "index.rst")

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

# -- LaTeX setup ---------------------------------------------------------------

if (CAF_BUILD_TEX_MANUAL)
  find_package(LATEX)
  message(STATUS "Add optional target: manual.")
  include("cmake/UseLATEX.cmake")
  # enable synctex for convenient editing
  set(LATEX_USE_SYNCTEX yes)
  # add manual.pdf as target
  add_latex_document(tex/manual.tex
                     INPUTS ${sources} "tex/variables.tex"
                     IMAGE_DIRS "pdf"
                     FORCE_PDF
                     TARGET_NAME manual)
  add_dependencies(doc manual)
endif()

