cmake_policy(SET CMP0015 NEW)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__STDC_WANT_LIB_EXT1__=1 -DUSE_SECURE_MEMZERO")


set(SOURCES
    keccak-tiny.h keccak-tiny.c
    nrghash.h nrghash.cpp
    secure_memzero.h
)

add_library(libnrghash ${SOURCES})
target_include_directories(libnrghash PRIVATE ..)
