#pragma once

#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "primitives/worker.h"
#include "nrgcore/miner.h"
#include "MiningClient.h"

typedef struct {
    std::string host;
    std::string port;
    std::string user;
    std::string pass;
} credentials;


class StratumClient : public MiningClient
{
public:
    StratumClient(energi::MinePlant* plant,
                  MinerExecutionMode mode,
                  const std::string& farmURL,
                  const std::string& m_user,
                  const std::string& m_pass,
                  unsigned maxretries,
                  int timeout);

    ~StratumClient();

public:
    bool submit(const energi::Solution& solution) override;
    void reconnect();
    Json::Value getBlockTemplate() throw (jsonrpc::JsonRpcException) override;
    energi::Work getWork() override;

public:
	void setFailover(const std::string& failOverURL);

private:
    void connect();
    void disconnect();
    bool processResponse(Json::Value& responseObject);

    void workTimeoutHandler(const boost::system::error_code& ec);

public:
    MinerExecutionMode m_minerType;

    credentials* p_active;
    credentials  m_primary;
    credentials  m_failover;

    bool m_authorized;
    bool m_connected;
    bool m_running = true;

    int	m_retries = 0;
    int	m_maxRetries;
    int m_worktimeout = 60;

    std::string m_response;

    energi::MinePlant* p_farm;
    energi::Work m_current;

    boost::asio::io_service m_io_service;
    boost::asio::ip::tcp::socket m_socket;

    boost::asio::streambuf m_requestBuffer;
    boost::asio::streambuf m_responseBuffer;

    boost::asio::deadline_timer m_worktimer;

    double m_nextWorkDifficulty;
    int m_extraNonceHexSize;

	void processExtranonce(std::string& enonce);
};
