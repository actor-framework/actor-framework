add_custom_target(all_tools)

macro(add name)
  add_executable(${name} ${name}.cpp ${ARGN})
  install(FILES ${name}.cpp DESTINATION ${CMAKE_INSTALL_DATADIR}/caf/tools)
  add_dependencies(${name} all_tools)
endmacro()

add(caf-vec)
target_link_libraries(caf-vec PRIVATE CAF::core)

if(TARGET CAF::io)
  if(WIN32)
    message(STATUS "skip caf-run (not supported on Windows)")
  else()
    add(caf-run)
    target_link_libraries(caf-run PRIVATE CAF::io CAF::core)
  endif()
endif()
