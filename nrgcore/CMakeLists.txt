cmake_policy(SET CMP0015 NEW)

if (NOT MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__STDC_WANT_LIB_EXT1__=1 -DUSE_SECURE_MEMZERO")
endif()

aux_source_directory(. SRC_LIST)

include_directories(BEFORE ..)

file(GLOB HEADERS "*.h")

add_library(libnrgcore ${SRC_LIST} ${SOURCES} ${HEADERS})
target_include_directories(libnrgcore PRIVATE ..)

if (HASHCL)
    target_link_libraries(libnrgcore nrghash-cl libnrghash)
endif()
if (HASHCUDA)
    target_link_libraries(libnrgcore nrghash-cuda libnrghash)
endif()
