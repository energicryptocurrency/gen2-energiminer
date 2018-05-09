cmake_policy(SET CMP0015 NEW)

if (NOT MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__STDC_WANT_LIB_EXT1__=1 -DUSE_SECURE_MEMZERO")
endif()

aux_source_directory(. SRC_LIST)

include_directories(BEFORE ..)

set(EXECUTABLE energiminer)

file(GLOB HEADERS "*.h")

set(SOURCES
  nrghash/nrghash.cpp
  nrghash/nrghash.h
  nrghash/keccak-tiny.h
  nrghash/keccak-tiny.c
  nrghash/secure_memzero.h
)



add_executable(${EXECUTABLE} ${SRC_LIST} ${SOURCES} ${HEADERS})

add_dependencies(${EXECUTABLE} BuildInfo.h )

target_link_libraries(${EXECUTABLE} libjson-rpc-cpp::client poolprotocols libprimitives libcommon Boost::boost Threads::Threads Boost::system)

if (ETHASHCL)
    target_link_libraries(${EXECUTABLE} egihash-cl)
endif()
if (ETHASHCUDA)
    target_link_libraries(${EXECUTABLE} nrghash-cuda)
endif()

include(GNUInstallDirs)
install(TARGETS energiminer DESTINATION ${CMAKE_INSTALL_BINDIR})
if(MSVC)
    install(FILES $<TARGET_PDB_FILE:energiminer> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
endif()
