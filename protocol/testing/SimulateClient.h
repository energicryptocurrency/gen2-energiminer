#pragma once

#include <iostream>
#include <primitives/worker.h>

#include "../PoolClient.h"

class SimulateClient : public PoolClient, energi::Worker
{
public:
    SimulateClient(const unsigned& difficulty, const unsigned& block);
    ~SimulateClient();

    void connect() override;
    void disconnect() override;

    bool isConnected() override { return m_connected;}
    bool isPendingState() override { return false;}
    std::string ActiveEndPoint() override { return "";}

    void submitHashrate(const std::string& rate) override;
	void submitSolution(const energi::Solution& solution) override;

private:
    void trun() override;
    bool m_uppDifficulty = false;
    unsigned m_difficulty;
    unsigned m_block;
    std::chrono::steady_clock::time_point m_time;

};
