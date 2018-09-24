#include <chrono>
#include <boost/exception/diagnostic_information.hpp>

#include "GetworkClient.h"

std::mutex GetworkClient::s_mutex;

using namespace energi;

GetworkClient::GetworkClient(unsigned const & farmRecheckPeriod, const std::string& coinbase)
    : PoolClient()
    , Worker("getwork")
    , m_coinbase(coinbase)
{
	m_farmRecheckPeriod = farmRecheckPeriod;
    m_subscribed.store(true, std::memory_order_relaxed);
    m_authorized.store(true, std::memory_order_relaxed);
}

GetworkClient::~GetworkClient()
{
	p_client = nullptr;
}

void GetworkClient::connect()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    std::string uri = "http://" + m_conn->User() + ":" + m_conn->Pass() + "@" + m_conn->Host() + ":" + std::to_string(m_conn->Port());
    if (m_conn->Path().length())
        uri += m_conn->Path();
    p_client = new ::JsonrpcGetwork(new jsonrpc::HttpClient(uri), m_coinbase);

    m_connected.store(true, std::memory_order_relaxed);
    // Since we do not have a real connected state with getwork, we just fake it
    if (m_onConnected) {
        m_onConnected();
    }
    // No need to worry about starting again.
    // Worker class prevents that
    startWorking();
}

void GetworkClient::disconnect()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    m_connected.store(false, std::memory_order_relaxed);

	// Since we do not have a real connected state with getwork, we just fake it.
	if (m_onDisconnected) {
		m_onDisconnected();
	}
    delete p_client;
    p_client = nullptr;
}

void GetworkClient::submitHashrate(const std::string& rate)
{
    std::lock_guard<std::mutex> lock(s_mutex);
	// Store the rate in temp var. Will be handled in workLoop
	m_currentHashrateToSubmit = rate;
}

void GetworkClient::submit()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (m_connected.load(std::memory_order_relaxed)) {
        try {
            m_prevWork.reset();
            std::chrono::steady_clock::time_point submit_start = std::chrono::steady_clock::now();
            bool accepted = p_client->submitWork(m_solutionToSubmit);
            std::chrono::milliseconds response_delay_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - submit_start);
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
    }
    m_solutionToSubmit.reset();
}

void GetworkClient::submitSolution(const Solution& solution)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    m_solutionToSubmit = solution;
}

// Handles all getwork communication.
void GetworkClient::trun()
{
    while (!shouldStop()) {
        if (m_connected.load(std::memory_order_relaxed)) {
            // Get Work
            try {
                if (m_solutionToSubmit.getNonce() > 0) {
                    submit();
                }
                energi::Work newWork = p_client->getWork();
                // Check if header changes so the new workpackage is really new
                if (newWork != m_prevWork) {
                    m_prevWork = newWork;
                    if (m_onWorkReceived) {
                        m_onWorkReceived(m_prevWork);
                    }
                }
            } catch (jsonrpc::JsonRpcException) {
                cwarn << "Failed getting work!";
                if (m_onResetWork) {
                    m_onResetWork();
                }
                disconnect();
            }
            //TODO submit hashrate part
        }
        // Sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(m_farmRecheckPeriod));
    }
    if (m_onDisconnected) {
        m_onDisconnected();
    }
}
