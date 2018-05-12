set(SOURCES
    common.h
    Log.h Log.cpp
    portable_endian.h
    prevector.h
    serialize.h
    streams.h
    Terminal.h
    tinyformat.h
    utilstrencodings.h utilstrencodings.cpp
    ZeroAfterFreeAllocator.h
)

add_library(libcommon ${SOURCES})
target_include_directories(libcommon PRIVATE ..)
