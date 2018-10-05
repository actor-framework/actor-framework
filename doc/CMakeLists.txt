cmake_minimum_required(VERSION 2.8.12)

project(doc NONE)

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
)

# -- process .in files -----------------------------------------------------

configure_file("cmake/Doxyfile.in"
               "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
               @ONLY)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tex")

configure_file("cmake/variables.tex.in"
               "${CMAKE_CURRENT_BINARY_DIR}/tex/variables.tex"
               @ONLY)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/rst")

configure_file("cmake/conf.py.in"
               "${CMAKE_CURRENT_BINARY_DIR}/rst/conf.py"
               @ONLY)

configure_file("cmake/index_footer.rst.in"
               "${CMAKE_CURRENT_BINARY_DIR}/rst/index_footer.rst"
               @ONLY)

configure_file("cmake/index_header.rst.in"
               "${CMAKE_CURRENT_BINARY_DIR}/rst/index_header.rst"
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

# -- Pandoc utility macro ------------------------------------------------------

macro(generate_rst texfile)
  get_filename_component(rstfile_we "${texfile}" NAME_WE)
  set(rstfile "${rstfile_we}.rst")
  set(bin_texfile "${CMAKE_CURRENT_BINARY_DIR}/${texfile}")
  add_custom_target("${rstfile}"
                    DEPENDS "${bin_texfile}"
                    COMMAND ${PANDOC_EXECUTABLE}
                            "--filter=${CMAKE_SOURCE_DIR}/scripts/pandoc-filter.py"
                            --wrap=none -f latex
                            -o "${CMAKE_CURRENT_BINARY_DIR}/rst/${rstfile}"
                            "${bin_texfile}"
                    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/rst")
  add_dependencies(rst "${rstfile}")
endmacro()

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
  find_program(PANDOC_EXECUTABLE pandoc)
  if(NOT EXISTS ${PANDOC_EXECUTABLE})
    message(STATUS "Pandoc not found, skip generating reFormattedText version of the manual.")
  else()
    execute_process(COMMAND "python" "-c"
                            "from pandocfilters import toJSONFilter; print('ok')"
                    RESULT_VARIABLE has_pandocfilters
                    OUTPUT_QUIET
                    ERROR_QUIET)
    if(NOT ${has_pandocfilters} EQUAL 0)
      message(STATUS "Python with pandocfilters not found, skip generating reFormattedText version of the manual.")
    else()
      message(STATUS "Add optional target: rst.")
      add_custom_target(rst)
      add_dependencies(doc rst)
      foreach(texfile ${sources})
        generate_rst(${texfile})
      endforeach()
    endif()
  endif()
endif()

