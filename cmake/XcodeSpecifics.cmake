#  -*- mode: cmake; coding: utf-8; indent-tabs-mode: nil; -*-
#
# Specifics of the Xcode generator
#
#########################################################################
#
# FUNCTION: xcode_assert_target_not_empty(target)
#
# If `target` was defined in conjunction with an object library
# (`TARGET_OBJECTS`), then the Xcode build system may delete the
# generated `target` artefact during the build. This occurs, when
# there are no regular sources in the `target`.
#
# Calling this function will generate a dummy source file, to make
# sure that the SOURCES property of `target` contains at least one
# regular source file.
#
# Applies only under the Xcode generator. For all other generators,
# the function does nothing.
#
# If `target` is not a valid target, does nothing.
#
function(xcode_assert_target_not_empty target)
  if(XCODE)
    if(TARGET ${target})
      set(dummy_content "namespace { [[maybe_unused]] auto dummy = 0; }\n")
      set(dummy_location "${CMAKE_CURRENT_BINARY_DIR}/src/${target}_dummy.cpp")
      file(GENERATE OUTPUT "${dummy_location}" CONTENT "${dummy_content}")
      if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.1)
        target_sources(${target} PRIVATE "${dummy_location}")
      else()
        set_property(TARGET ${target} APPEND PROPERTY SOURCES "${dummy_location}")
      endif()
    else(TARGET ${target})
      message(WARNING "xcode_assert_target_not_empty(): Not a target: ${target}")
    endif(TARGET ${target})
  endif(XCODE)
endfunction(xcode_assert_target_not_empty)
