set(SOURCES
    PoolClient.h
    PoolURI.h PoolURI.cpp
    PoolManager.h PoolManager.cpp
    getwork/jsonrpc_getwork.h
    getwork/GetworkClient.h
    getwork/GetworkClient.cpp
    stratum/EthStratumClient.h
    stratum/EthStratumClient.cpp
)

hunter_add_package(OpenSSL)
find_package(OpenSSL REQUIRED)

add_library(poolprotocols ${SOURCES})
target_link_libraries(poolprotocols PRIVATE libnrgcore libjson-rpc-cpp::client Boost::system jsoncpp_lib_static network-uri OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(poolprotocols PRIVATE ..)
