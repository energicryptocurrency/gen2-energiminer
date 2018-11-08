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
    m_subscribed.store(true, std::memory_order_release);
    m_authorized.store(true, std::memory_order_release);
}

GetworkClient::~GetworkClient()
{}

void GetworkClient::connect()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    std::string uri = "http://" + m_conn->User() + ":" + m_conn->Pass() + "@" + m_conn->Host() + ":" + std::to_string(m_conn->Port());
    if (m_conn->Path().length())
        uri += m_conn->Path();

    m_client.reset(new ::JsonrpcGetwork(new jsonrpc::HttpClient(uri), m_coinbase));

    m_connected.store(true, std::memory_order_release);
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
    m_connected.store(false, std::memory_order_release);

	// Since we do not have a real connected state with getwork, we just fake it.
	if (m_onDisconnected) {
		m_onDisconnected();
	}

	m_client.reset();
}

void GetworkClient::submitHashrate(const std::string& rate)
{
    //std::lock_guard<std::mutex> lock(s_mutex);
	// Store the rate in temp var. Will be handled in workLoop
	//m_currentHashrateToSubmit = rate;
}

void GetworkClient::submitSolution(const Solution& solution)
{
    if (m_onResetWork) {
        m_onResetWork();
    }

    std::lock_guard<std::mutex> lock(s_mutex);

    if (!m_prevWork.isValid()) {
        return;
    }

    if (m_connected.load(std::memory_order_relaxed)) {
        try {
            std::chrono::steady_clock::time_point submit_start = std::chrono::steady_clock::now();
            bool accepted = m_client->submitWork(solution);
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

    m_prevWork.reset();
}


// Handles all getwork communication.
void GetworkClient::trun()
{
    while (!shouldStop()) {
        if (m_connected.load(std::memory_order_relaxed)) {
            // Get Work
            try {
                std::lock_guard<std::mutex> lock(s_mutex);
                auto newWork = m_client->getWork();
                
                std::atomic_thread_fence(std::memory_order_acquire);

                // Check if header changes so the new workpackage is really new
                if (newWork != m_prevWork) {
                    m_prevWork = newWork;
                    
                    if (m_onWorkReceived) {
                        cnote << "Difficulty set to: " << newWork.hashTarget.GetHex();
                        m_onWorkReceived(m_prevWork);
                    }
                }
            } catch (jsonrpc::JsonRpcException &e) {
                cwarn << "Failed getting work! " << e.what();
                if (m_onResetWork) {
                    m_onResetWork();
                }
            }
            //TODO submit hashrate part
        }
        // Sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(m_farmRecheckPeriod));
    }

    disconnect();
}
