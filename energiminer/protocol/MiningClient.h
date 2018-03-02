#pragma once

/**
 * @class MiningClient
 * @description base class for all type mining clients
 */

#include <jsonrpccpp/client.h>

//! TODO add future implementation
class MiningClient
{
public:
    /**
     * @brief submit founded solution
     * @param solution founded solution
     */
    virtual bool submit(const energi::Solution& solution) = 0;

    /**
     * @brief getBlockTemplate returns the 'getbktemplate' funccion resul
     */
    virtual Json::Value getBlockTemplate() throw (jsonrpc::JsonRpcException) = 0;

    /**
     * @brief getWork returns the new constructed job
     */
    virtual energi::Work getWork() = 0;
};
