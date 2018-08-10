set(SOURCES
    PoolClient.h
    PoolURI.h PoolURI.cpp
    PoolManager.h PoolManager.cpp
    getwork/jsonrpc_getwork.h
    getwork/GetworkClient.h
    getwork/GetworkClient.cpp
    stratum/StratumClient.h
    stratum/StratumClient.cpp
)

hunter_add_package(OpenSSL)
find_package(OpenSSL REQUIRED)

add_library(poolprotocols ${SOURCES})
target_link_libraries(poolprotocols PRIVATE energiminer-buildinfo libnrgcore libjson-rpc-cpp::client Boost::system jsoncpp_lib_static network-uri OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(poolprotocols PRIVATE ..)
