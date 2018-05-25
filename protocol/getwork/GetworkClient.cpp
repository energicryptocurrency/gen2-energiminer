#include <chrono>
#include <boost/exception/diagnostic_information.hpp>

#include "GetworkClient.h"

using namespace energi;

GetworkClient::GetworkClient(unsigned const & farmRecheckPeriod, const std::string& coinbase)
    : PoolClient()
    , Worker("getwork")
    , m_coinbase(coinbase)
{
	m_farmRecheckPeriod = farmRecheckPeriod;
	m_authorized = true;
	m_connection_changed = true;
    m_solutionToSubmit.reset();
    startWorking();
}

GetworkClient::~GetworkClient()
{
	p_client = nullptr;
}

void GetworkClient::connect()
{
    if (m_connection_changed) {
        std::stringstream ss;
        ss <<  "http://" + m_conn.User()  << ":" << m_conn.Pass() << "@" << m_conn.Host() << ':' << m_conn.Port();
        if (m_conn.Path().length())
            ss << m_conn.Path();
        p_client = new ::JsonrpcGetwork(new jsonrpc::HttpClient(ss.str()), m_coinbase);
    }
    //	cnote << "connect to " << m_host;
    m_connection_changed = false;
    m_justConnected = true; // We set a fake flag, that we can check with workhandler if connection works
}

void GetworkClient::disconnect()
{
	m_connected = false;
	m_justConnected = false;

	// Since we do not have a real connected state with getwork, we just fake it.
	if (m_onDisconnected) {
		m_onDisconnected();
	}
}

void GetworkClient::submitHashrate(const std::string& rate)
{
	// Store the rate in temp var. Will be handled in workLoop
	m_currentHashrateToSubmit = rate;
}

void GetworkClient::submitSolution(Solution solution)
{
	// Store the solution in temp var. Will be handled in workLoop
	m_solutionToSubmit = solution;
}

// Handles all getwork communication.
void GetworkClient::trun()
{
    while (true) {
        if (m_connected || m_justConnected) {
            // Submit solution
            if (m_solutionToSubmit.getNonce()) {
                try {
                    bool accepted = p_client->submitWork(m_solutionToSubmit);
                    if (accepted) {
                        if (m_onSolutionAccepted) {
                            m_onSolutionAccepted(false);
                        }
                    } else {
                        if (m_onSolutionRejected) {
                            m_onSolutionRejected(false);
                        }
                    }
                    m_solutionToSubmit.reset();
                    //m_solutionToSubmit.nonce = 0;
                } catch (const jsonrpc::JsonRpcException& ex) {
                    cwarn << "Failed to submit solution.";
                    cwarn << boost::diagnostic_information(ex);
                }
            }
            // Get Work
            try {
                energi::Work newWork = p_client->getWork();
                // Since we do not have a real connected state with getwork, we just fake it.
                // If getting work succeeds we know that the connection works
                if (m_justConnected && m_onConnected) {
                    m_justConnected = false;
                    m_connected = true;
                    m_onConnected();
                }

                // Check if header changes so the new workpackage is really new
                if (newWork != m_prevWork) {
                    m_prevWork = newWork;
                    if (m_onWorkReceived) {
                        m_onWorkReceived(m_prevWork);
                    }
                }
            } catch (jsonrpc::JsonRpcException) {
                cwarn << "Failed getting work!";
                disconnect();
            }
            //TODO submit hashrate part
        }
        // Sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(m_farmRecheckPeriod));
    }
}
