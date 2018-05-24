#pragma once

#include <jsonrpccpp/client/connectors/httpclient.h>
#include <iostream>
#include <primitives/worker.h>
#include "jsonrpc_getwork.h"
#include "../PoolClient.h"

class GetworkClient : public PoolClient, energi::Worker
{
public:
	GetworkClient(unsigned const & farmRecheckPeriod, const std::string& coinbase);
	~GetworkClient();

	void connect() override;
	void disconnect() override;

	bool isConnected() override { return m_connected; }
    std::string ActiveEndPoint() override { return ""; };

	void submitSolution(energi::Solution solution) override;

private:
	void trun() override;
	unsigned m_farmRecheckPeriod = 500;

    std::string m_coinbase;
    std::string m_currentHashrateToSubmit = "";
    energi::Solution m_solutionToSubmit;
	bool m_justConnected = false;
	JsonrpcGetwork *p_client;
    energi::Work m_prevWork;
};
