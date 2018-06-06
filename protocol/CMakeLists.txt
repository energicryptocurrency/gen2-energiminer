set(SOURCES
    MiningClient.h
    GBTClient.h
    StratumClient.h
    StratumClient.cpp
)

add_library(poolprotocols ${SOURCES})
target_link_libraries(poolprotocols PRIVATE libnrgcore libjson-rpc-cpp::client Boost::system jsoncpp_lib_static)
target_include_directories(poolprotocols PRIVATE ..)
