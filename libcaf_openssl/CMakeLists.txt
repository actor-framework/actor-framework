project(caf_openssl C CXX)

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB_RECURSE LIBCAF_OPENSSL_HDRS "caf/*.hpp")

# list cpp files excluding platform-dependent files
set(LIBCAF_OPENSSL_SRCS
  src/openssl/manager.cpp
  src/openssl/middleman_actor.cpp
  src/openssl/publish.cpp
  src/openssl/remote_actor.cpp
  src/openssl/session.cpp
)

add_library(libcaf_openssl ${LIBCAF_OPENSSL_SRCS} ${LIBCAF_OPENSSL_HDRS})
target_link_libraries(libcaf_openssl PUBLIC caf::core caf::io ${OPENSSL_LIBRARIES})
if(NOT APPLE AND NOT WIN32)
  target_link_libraries(libcaf_openssl PUBLIC "-pthread")
endif()
target_include_directories(libcaf_openssl PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(libcaf_openssl
  PROPERTIES
  EXPORT_NAME openssl
  SOVERSION ${CAF_VERSION}
  VERSION ${CAF_LIB_VERSION}
  OUTPUT_NAME caf_openssl
)

install(TARGETS libcaf_openssl
  EXPORT CAFTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT openssl
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT openssl
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT openssl
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT openssl
        FILES_MATCHING PATTERN "*.hpp"
)

add_library(caf::openssl ALIAS libcaf_openssl)
if (BUILD_SHARED_LIBS)
  add_library(libcaf_openssl_shared ALIAS libcaf_openssl)
else ()
  add_library(libcaf_openssl_static ALIAS libcaf_openssl)
endif ()
list(APPEND CAF_LIBRARIES libcaf_openssl)
set(CAF_LIBRARIES ${CAF_LIBRARIES} PARENT_SCOPE)
