#pragma once

#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "energiminer/worker.h"
#include "energiminer/miner.h"

typedef struct {
    std::string host;
    std::string port;
    std::string user;
    std::string pass;
} credentials;


class StratumClient : public energi::Worker
{
public:
    StratumClient(energi::MinePlant* plant,
                  MinerExecutionMode mode,
                  const std::string& farmURL,
                  unsigned maxretries,
                  int timeout);
    ~StratumClient();

public:
    bool isRunning() { return m_running; }
    bool isConnected() { return m_connected && m_authorized; }
    bool current() { return this->work().isValid(); }

    bool submitHashrate(const::std::string& rate);
    bool submit(const energi::Solution& solution);
    void reconnect();

public:
	void setFailover(const std::string& failOverURL);

private:
    void trun() override;
    void connect();
    void disconnect();
    void processReponse(Json::Value& responseObject);

    void work_timeout_handler(const boost::system::error_code& ec);

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
    energi::Miner m_current;

    boost::asio::io_service m_io_service;
    boost::asio::ip::tcp::socket m_socket;

    boost::asio::streambuf m_requestBuffer;
    boost::asio::streambuf m_responseBuffer;

    boost::asio::deadline_timer m_worktimer;

    //string m_email;

    double m_nextWorkDifficulty;

    //h64 m_extraNonce;
    int m_extraNonceHexSize;

    void processExtranonce(std::string& enonce);
};
