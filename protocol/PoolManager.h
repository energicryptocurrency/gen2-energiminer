#pragma once

#include <iostream>
#include <primitives/worker.h>
#include <nrgcore/mineplant.h>
#include <nrgcore/miner.h>

#include "PoolClient.h"

class PoolManager : public energi::Worker
{
public:
    PoolManager(boost::asio::io_service & io_service,
                PoolClient* client,
                energi::MinePlant& farm,
                const MinerExecutionMode& minerType,
                unsigned maxTries,
                unsigned failovertimeout);
    void addConnection(URI &conn);
    void clearConnections();
    bool start();
    void stop();

    bool isConnected() { return p_client->isConnected(); };
    bool isRunning() { return m_running; };

private:
    unsigned m_hashrateReportingTime = 60;
    unsigned m_hashrateReportingTimePassed = 0;
    // After this amount of time in minutes of mining on a failover pool return to "primary"
    unsigned m_failoverTimeout = 0;
    void check_failover_timeout(const boost::system::error_code& ec);

    std::atomic<bool> m_running = { false };
    void trun() override;
    unsigned m_connectionAttempt = 0;
    unsigned m_maxConnectionAttempts = 0;
    unsigned m_activeConnectionIdx = 0;

    std::vector <URI> m_connections;
    std::thread m_workThread;

    boost::asio::io_service::strand m_io_strand;
    boost::asio::deadline_timer m_failovertimer;
    PoolClient *p_client;
    energi::MinePlant &m_farm;
    MinerExecutionMode m_minerType;
    std::chrono::steady_clock::time_point m_submit_time;
};
