#include <chrono>
#include <boost/algorithm/string.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/client.h>

#include "GetworkClient.h"
#include <common/Log.h>

using namespace energi;

GetworkClient::GetworkClient(unsigned const & farmRecheckPeriod, const std::string& coinbase)
    : PoolClient()
    , Worker("getwork")
    , m_coinbase(coinbase)
{
	m_farmRecheckPeriod = farmRecheckPeriod;
    m_subscribed.store(true, std::memory_order_release);
    m_authorized.store(true, std::memory_order_release);
}

GetworkClient::~GetworkClient()
{}

void GetworkClient::connect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string uri = "http://" + m_conn->User() + ":" + m_conn->Pass() + "@" + m_conn->Host() + ":" + std::to_string(m_conn->Port());
    if (m_conn->Path().length())
        uri += m_conn->Path();

    m_url = uri;
    m_display_url = boost::replace_first_copy(uri, m_conn->Pass(), "<password>");

    m_connected.store(true, std::memory_order_release);

    // No need to worry about starting again.
    // Worker class prevents that
    startWorking();
}

void GetworkClient::disconnect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connected.store(false, std::memory_order_release);

	// Since we do not have a real connected state with getwork, we just fake it.
	if (m_onDisconnected) {
		m_onDisconnected();
	}
}

void GetworkClient::submitHashrate(const std::string& rate)
{
}

void GetworkClient::submitSolution(const Solution& solution)
{
    do {
        std::lock_guard<std::mutex> lock(m_mutex);

        //---
        if (!m_have_work) {
            break;
        }
        
        if (m_onResetWork) {
            m_onResetWork();
        }
        
        m_have_work = false;

        //---
        if (!m_connected.load(std::memory_order_relaxed)) {
            break;
        }

        try {
            jsonrpc::HttpClient http_client(m_url);
            jsonrpc::Client client(http_client, jsonrpc::JSONRPC_CLIENT_V1);
            
            Json::Value params(Json::arrayValue);
            auto block = solution.getSubmitBlockData();
            params.append(block);


            //---
            std::chrono::steady_clock::time_point submit_start = std::chrono::steady_clock::now();

            Json::Value result = client.CallMethod("submitblock", params);

            std::chrono::milliseconds response_delay_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - submit_start);
            
            //---
            auto resultStr = result.toStyledString();
            resultStr.pop_back(); // includes a newline
            
            if (resultStr == "null") {
                cnote << "Block Accepted";
            } else if (resultStr == "inconclusive") {
                cnote << "Block too old";
            } else {
                cnote << "Block Rejected:" << resultStr;
            }

            //---
            bool accepted = (resultStr == "null");

            if (accepted) {
                if (m_onSolutionAccepted) {
                    m_onSolutionAccepted(false, response_delay_ms);
                }
            } else {
                if (m_onSolutionRejected) {
                    m_onSolutionRejected(false, response_delay_ms);
                }
            }
        } catch (const jsonrpc::JsonRpcException& ex) {
            cwarn << "Failed to submit solution.";
            cwarn << boost::diagnostic_information(ex);
        }
    } while (false);

    m_cvwait.notify_one();
}


// Handles all getwork communication.
void GetworkClient::trun()
{
    jsonrpc::HttpClient http_client(m_url);
    jsonrpc::Client client(http_client, jsonrpc::JSONRPC_CLIENT_V1);
    
    // Since we do not have a real connected state with getwork, we just fake it
    if (m_onConnected) {
        m_onConnected();
    }
    
    const auto NOWORK_DIV = 10U;
    const std::chrono::milliseconds farm_recheck{m_farmRecheckPeriod};
    const std::chrono::milliseconds nowork_delay{
        m_farmRecheckPeriod > NOWORK_DIV ? m_farmRecheckPeriod / NOWORK_DIV : m_farmRecheckPeriod
    };
    energi::Work current_work;

    while (!shouldStop()) {
        if (!m_connected.load(std::memory_order_relaxed)) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cvwait.wait_for(lock, farm_recheck);
        }

        try {
            //---
            auto params = Json::Value(Json::arrayValue);
            auto object = Json::Value(Json::objectValue);
            object["capabilities"] = Json::Value(Json::arrayValue);
            object["capabilities"].append("coinbasetxn");
            object["capabilities"].append("coinbasevalue");
            object["capabilities"].append("longpoll");
            object["capabilities"].append("workid");
            params.append(object);

            Json::Value workGBT = client.CallMethod("getblocktemplate", params);

            if (!workGBT.isObject() ) {
                throw jsonrpc::JsonRpcException(
                        jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE,
                        workGBT.toStyledString());
            }

            //---
            auto new_work = energi::Work(workGBT, m_coinbase);
            
            std::unique_lock<std::mutex> lock(m_mutex);

            if (current_work != new_work) {
                if (m_onWorkReceived) {
                    current_work = new_work;
                    m_have_work = true;

                    cnote << "Difficulty set to: " << new_work.hashTarget.GetHex();
                    m_onWorkReceived(new_work);
                }
            } else if (m_have_work) {
                m_cvwait.wait_for(lock, farm_recheck);
            } else {
                m_cvwait.wait_for(lock, nowork_delay);
            }
        } catch (jsonrpc::JsonRpcException ex) {
            cwarn << "Failed getting work! ";
            cwarn << boost::diagnostic_information(ex);

            if (m_onResetWork) {
                m_onResetWork();
            }
        }
    }

    disconnect();
}
