cmake_policy(SET CMP0015 NEW)

if (NOT MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__STDC_WANT_LIB_EXT1__=1 -DUSE_SECURE_MEMZERO")
endif()

aux_source_directory(. SRC_LIST)

include_directories(BEFORE ..)

set(EXECUTABLE energiminer)

file(GLOB HEADERS "*.h")

set(SOURCES
  protocol/MiningClient.h
  protocol/GBTClient.h
  protocol/StratumClient.h
  protocol/StratumClient.cpp
  primitives/hash.h
  primitives/base58.h
  primitives/base58.cpp
  primitives/sha256.h
  primitives/sha256.cpp
  primitives/uint256.h
  primitives/uint256.cpp
  primitives/byteswap.h
  primitives/block.h
  primitives/arith_uint256.h
  primitives/arith_uint256.cpp
  primitives/script.h
  primitives/script.cpp
  primitives/prevector.h
  primitives/transaction.h
  primitives/transaction.cpp
  primitives/amount.h
  primitives/amount.cpp
  common/serialize.h
  common/common.h
  common/Log.h
  common/Log.cpp
  common/Terminal.h
  common/utilstrencodings.h
  common/utilstrencodings.cpp
  common/merkle.h
  common/merkle.cpp
  nrghash/nrghash.cpp
  nrghash/nrghash.h
  nrghash/keccak-tiny.h
  nrghash/keccak-tiny.c
  nrghash/secure_memzero.h
)



add_executable(${EXECUTABLE} ${SRC_LIST} ${SOURCES} ${HEADERS})

add_dependencies(${EXECUTABLE} BuildInfo.h )

target_link_libraries(${EXECUTABLE} libjson-rpc-cpp::client  Boost::boost Threads::Threads Boost::system egihash-cl)

include(GNUInstallDirs)
install(TARGETS energiminer DESTINATION ${CMAKE_INSTALL_BINDIR})
if(MSVC)
    install(FILES $<TARGET_PDB_FILE:energiminer> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
endif()
