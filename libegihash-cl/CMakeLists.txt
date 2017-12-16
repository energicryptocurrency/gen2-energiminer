# A custom command and target to turn the OpenCL kernel into a byte array header
# The normal build depends on it properly and if the kernel file is changed, then
# a rebuild of libcl should be triggered
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/CLMiner_kernel.h
	COMMAND ${CMAKE_COMMAND} ARGS
	-DBIN2H_SOURCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/CLMiner_kernel.cl"
	-DBIN2H_VARIABLE_NAME=CLMiner_kernel
	-DBIN2H_HEADER_FILE="${CMAKE_CURRENT_BINARY_DIR}/CLMiner_kernel.h"
	-P "${CMAKE_CURRENT_SOURCE_DIR}/bin2h.cmake"
	COMMENT "Generating OpenCL Kernel Byte Array"
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/CLMiner_kernel.cl
)
add_custom_target(clbin2h DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/CLMiner_kernel.h ${CMAKE_CURRENT_SOURCE_DIR}/CLMiner_kernel.cl)

set(SOURCES
	OpenCLMiner.h OpenCLMiner.cpp
	${CMAKE_CURRENT_BINARY_DIR}/CLMiner_kernel.h
)

if(APPLE)
	# On macOS use system OpenCL library.
	find_package(OpenCL REQUIRED)
else()
	hunter_add_package(OpenCL)
	find_package(OpenCL CONFIG REQUIRED)
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(..)

add_library(egihash-cl ${SOURCES})
target_link_libraries(egihash-cl PUBLIC  Boost::system Boost::filesystem)
target_link_libraries(egihash-cl PRIVATE OpenCL::OpenCL)
