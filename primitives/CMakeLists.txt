set(SOURCES
    amount.h amount.cpp
    arith_uint256.h arith_uint256.cpp
    base58.h base58.cpp
    block.h
    byteswap.h
    hash.h
    script.h script.cpp
    sha256.h sha256.cpp
    transaction.h transaction.cpp
    uint256.h uint256.cpp
    merkle.h merkle.cpp
)

add_library(libprimitives ${SOURCES})
target_link_libraries(libprimitives PRIVATE jsoncpp_lib_static)
target_include_directories(libprimitives PRIVATE ..)
