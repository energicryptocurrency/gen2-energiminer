#pragma once

#include <iostream>
#include <memory>
#include <condition_variable>

#include <primitives/worker.h>
#include "../PoolClient.h"

class GetworkClient : public PoolClient, energi::Worker
{
public:
	GetworkClient(unsigned const & farmRecheckPeriod, const std::string& coinbase);
	~GetworkClient();

	void connect() override;
	void disconnect() override;

	bool isConnected() override { return m_connected; }
    bool isPendingState() override { return false; }

    std::string ActiveEndPoint() override { return m_display_url; };

	void submitHashrate(const std::string& rate) override;
	void submitSolution(const energi::Solution& solution) override;

private:
	void trun() override;
	unsigned m_farmRecheckPeriod = 500;

    std::string m_coinbase;
    std::string m_url;
    std::string m_display_url;

    std::mutex m_mutex;
    std::condition_variable m_cvwait;
    bool m_have_work = false;
};
