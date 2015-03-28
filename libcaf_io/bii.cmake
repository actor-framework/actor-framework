# build a shared library
set(BII_LIB_TYPE SHARED)

# add the biicode targets
add_biicode_targets()

# borrow some useful functions from the biicode/cmake block
include(biicode/cmake/tools) 
activate_cpp11(INTERFACE ${BII_BLOCK_TARGET})

# build with extra warnings
target_compile_options(${BII_BLOCK_TARGET} INTERFACE "-Wextra;-Wall;-pedantic")

# link against pthreads
if(NOT APPLE AND NOT WIN32)
	target_link_libraries(${BII_BLOCK_TARGET} INTERFACE pthread)
endif()

# extra setup steps needed on MinGW
if(MINGW)
  target_compile_definitions(${BII_BLOCK_TARGET} PUBLIC "_WIN32_WINNT=0x0600")
  target_compile_definitions(${BII_BLOCK_TARGET} PUBLIC "WIN32")
  include(GenerateExportHeader)
	target_link_libraries(${BII_BLOCK_TARGET} INTERFACE ws2_32)
	target_link_libraries(${BII_BLOCK_TARGET} INTERFACE liphlpapi)
endif()

################################################################################
#            add custom settings for library                                   #
################################################################################

if(CAF_LOG_LEVEL)
	target_compile_definitions(${BII_BLOCK_TARGET} PUBLIC 
		"CAF_LOG_LEVEL=${CAF_LOG_LEVEL}")
endif()

if(CAF_ENABLE_RUNTIME_CHECKS)
	target_compile_definitions(${BII_BLOCK_TARGET} PUBLIC 
		"CAF_ENABLE_RUNTIME_CHECKS=${DCAF_ENABLE_RUNTIME_CHECKS}")
endif()

if(CAF_NO_MEM_MANAGEMENT)
	target_compile_definitions(${BII_BLOCK_TARGET} PUBLIC 
		"CAF_NO_MEM_MANAGEMENT=${DCAF_NO_MEM_MANAGEMENT}")
endif()
