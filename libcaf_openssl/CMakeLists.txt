project(caf_openssl C CXX)

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB_RECURSE LIBCAF_OPENSSL_HDRS "caf/*.hpp")

# list cpp files excluding platform-dependent files
set(LIBCAF_OPENSSL_SRCS
  src/manager.cpp
  src/middleman_actor.cpp
  src/publish.cpp
  src/remote_actor.cpp
  src/session.cpp
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
  VERSION ${CAF_VERSION}
  OUTPUT_NAME caf_openssl
)

install(TARGETS libcaf_openssl
  EXPORT CAFTargets
  ARCHIVE DESTINATION lib COMPONENT openssl
  RUNTIME DESTINATION bin COMPONENT openssl
  LIBRARY DESTINATION lib COMPONENT openssl
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
        DESTINATION include
        COMPONENT openssl
        FILES_MATCHING PATTERN "*.hpp"
)

add_library(caf::openssl ALIAS libcaf_openssl)
list(APPEND CAF_LIBRARIES libcaf_openssl)
set(CAF_LIBRARIES ${CAF_LIBRARIES} PARENT_SCOPE)
