#pragma once

#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <json/json.h>
#include <common/Log.h>
#include <nrgcore/mineplant.h>
#include <nrgcore/miner.h>
#include "../PoolClient.h"

using namespace energi;

class StratumClient : public PoolClient
{
public:

	typedef enum { STRATUM = 0, ETHPROXY, ETHEREUMSTRATUM } StratumProtocol;

	StratumClient(boost::asio::io_service& io_service, int worktimeout, int responsetimeout, const std::string& email, bool submitHashrate);
	~StratumClient();

	void connect() override;
	void disconnect() override;

	// Connected and Connection Statuses
	bool isConnected() override
	{
        return m_connected.load(std::memory_order_relaxed) && !isPendingState();
	}

    bool isPendingState() override
    {
        return (m_connecting.load(std::memory_order_relaxed) || m_disconnecting.load(std::memory_order_relaxed));
    }
    std::string ActiveEndPoint() override { return " [" + toString(m_endpoint) + "]"; };

	void submitHashrate(const std::string& rate) override;
	void submitSolution(const energi::Solution& solution) override;

private:
    void disconnect_finalize();

    void resolve_handler(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator i);
    void start_connect();
    void check_connect_timeout(const boost::system::error_code& ec);
    void connect_handler(const boost::system::error_code& ec);
    void work_timeout_handler(const boost::system::error_code& ec);
    void response_timeout_handler(const boost::system::error_code& ec);

    void reset_work_timeout();
    void processResponse(Json::Value& responseObject);
    std::string processError(Json::Value& erroresponseObject);
    void processExtranonce(std::string& enonce);

    void recvSocketData();
    void onRecvSocketDataCompleted(const boost::system::error_code& ec, std::size_t bytes_transferred);
    void sendSocketData(Json::Value const & jReq);
    void onSendSocketDataCompleted(const boost::system::error_code& ec);

    void onSSLShutdownCompleted(const boost::system::error_code& ec);

    std::string m_user;    // Only user part
    std::string m_worker; // eth-proxy only; No ! It's for all !!!

    std::atomic<bool> m_disconnecting = { false };
    std::atomic<bool> m_connecting = { false };

    // seconds to trigger a work_timeout (overwritten in constructor)
    int m_worktimeout;

    // seconds timeout for responses and connection (overwritten in constructor)
    int m_responsetimeout;

    energi::Work m_current;

    boost::asio::io_service& m_io_service;  // The IO service reference passed in the constructor
    boost::asio::io_service::strand m_io_strand;
    boost::asio::ip::tcp::socket *m_socket;

    // Use shared ptrs to avoid crashes due to async_writes
    // see https://stackoverflow.com/questions/41526553/can-async-write-cause-segmentation-fault-when-this-is-deleted
    std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>  m_securesocket;
    std::shared_ptr<boost::asio::ip::tcp::socket> m_nonsecuresocket;

    boost::asio::streambuf m_sendBuffer;
    boost::asio::streambuf m_recvBuffer;
    Json::FastWriter m_jWriter;

    boost::asio::deadline_timer m_conntimer;
    boost::asio::deadline_timer m_worktimer;
    boost::asio::deadline_timer m_responsetimer;
    bool m_response_pending = false;

    boost::asio::ip::tcp::resolver m_resolver;
    std::queue<boost::asio::ip::basic_endpoint<boost::asio::ip::tcp>> m_endpoints;

    std::string m_email;

    arith_uint256 m_nextWorkTarget = arith_uint256("0xffff000000000000000000000000000000000000000000000000000000000000");

    std::string m_extraNonce;
    int m_extraNonceHexSize = 0;

    bool m_submit_hashrate;
    std::string m_submit_hashrate_id;
};
