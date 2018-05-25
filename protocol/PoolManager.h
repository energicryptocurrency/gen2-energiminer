#pragma once

#include <iostream>
#include <primitives/worker.h>
#include <nrgcore/mineplant.h>
#include <nrgcore/miner.h>

#include "PoolClient.h"

class PoolManager : public energi::Worker
{
public:
    PoolManager(PoolClient* client,
                energi::MinePlant& farm,
                const MinerExecutionMode& minerType);
    void addConnection(URI &conn);
    void clearConnections();
    bool start();
    void stop();
    void setReconnectTries(const unsigned& reconnectTries)
    {
        m_reconnectTries = reconnectTries;
    };

    bool isConnected() { return p_client->isConnected(); };
    bool isRunning() { return m_running; };

private:
    unsigned m_hashrateReportingTime = 60;
    unsigned m_hashrateReportingTimePassed = 0;

    bool m_running = false;
    void trun() override;
    unsigned m_reconnectTries = 3;
    unsigned m_reconnectTry = 0;
    std::vector<URI> m_connections;
    unsigned m_activeConnectionIdx = 0;

    PoolClient *p_client;
    energi::MinePlant &m_farm;
    MinerExecutionMode m_minerType;
    std::chrono::steady_clock::time_point m_submit_time;
    void tryReconnect();
};
