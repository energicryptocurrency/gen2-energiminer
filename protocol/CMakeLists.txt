set(SOURCES
    MiningClient.h
    GBTClient.h
    StratumClient.h
    StratumClient.cpp
)

hunter_add_package(OpenSSL)
find_package(OpenSSL REQUIRED)

add_library(poolprotocols ${SOURCES})
target_link_libraries(poolprotocols PRIVATE libnrgcore libjson-rpc-cpp::client Boost::system jsoncpp_lib_static OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(poolprotocols PRIVATE ..)
