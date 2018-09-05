#include <chrono>
#include <boost/bind.hpp>

#include "PoolManager.h"

using namespace energi;

PoolManager::PoolManager(boost::asio::io_service& io_service,
                         PoolClient* client,
                         energi::MinePlant &farm,
                         const MinerExecutionMode& minerType,
                         unsigned maxTries,
                         unsigned failoverTimeout)
    : Worker("main")
    , m_io_strand(io_service)
    , m_failovertimer(io_service)
    , m_farm(farm)
    , m_minerType(minerType)
{
	p_client = client;
    m_maxConnectionAttempts = maxTries;
    m_failoverTimeout = failoverTimeout;

	p_client->onConnected([&]()
	{
        m_connectionAttempt = 0;
        m_activeConnectionHost = m_connections[m_activeConnectionIdx].Host();
        cnote << "Connected to " << m_connections[m_activeConnectionIdx].Host() << p_client->ActiveEndPoint();
        // Rough implementation to return to primary pool
        // after specified amount of time
        if (m_activeConnectionIdx != 0 && m_failoverTimeout > 0) {
            m_failovertimer.expires_from_now(boost::posix_time::minutes(m_failoverTimeout));
            m_failovertimer.async_wait(m_io_strand.wrap(boost::bind(&PoolManager::check_failover_timeout, this, boost::asio::placeholders::error)));
        } else {
            m_failovertimer.cancel();
        }

        if (!m_farm.isMining()) {
            cnote << "Spinning up miners...";
            auto vEngineModes = getEngineModes(m_minerType);
            m_farm.start(vEngineModes);
    }
	});
	p_client->onDisconnected([&]()
	{
        setThreadName("main");
        cnote << "Disconnected from " + m_activeConnectionHost << p_client->ActiveEndPoint();
        // Do not stop mining here
        // Workloop will determine if we're trying a fast reconnect to same pool
        // or if we're switching to failover(s)
	});
    p_client->onWorkReceived([&](const Work& wp)
    {
        m_farm.setWork(wp);
    });
	p_client->onSolutionAccepted([&](const bool& stale, const std::chrono::milliseconds& elapsedMs)
	{
		using namespace std::chrono;
		std::stringstream ss;
		ss << std::setw(4) << std::setfill(' ') << elapsedMs.count();
		ss << " ms." << "   " << m_connections[m_activeConnectionIdx].Host() + p_client->ActiveEndPoint();
		cnote << EthLime "**Accepted  " EthReset << (stale ? "(stale)" : "") << ss.str();
		m_farm.acceptedSolution(stale);
	});
	p_client->onSolutionRejected([&](const bool& stale, std::chrono::milliseconds const& elapsedMs)
	{
		using namespace std::chrono;
		std::stringstream ss;
		ss << std::setw(4) << std::setfill(' ') << elapsedMs.count();
		ss << " ms." << "   " << m_connections[m_activeConnectionIdx].Host() + p_client->ActiveEndPoint();
		cwarn << EthRed "**Rejected  " EthReset << (stale ? "(stale)" : "") << ss.str();
		m_farm.rejectedSolution();
	});

	m_farm.onSolutionFound([&](const Solution& sol)
	{
        // Solution should passthrough only if client is
        // properly connected. Otherwise we'll have the bad behavior
        // to log nonce submission but receive no response
        if (p_client->isConnected()) {
            p_client->submitSolution(sol);
        } else {
            cnote << std::string(EthRed "Nonce ") + std::to_string(sol.getNonce()) << " wasted. Waiting for connection ...";
        }
    return false;
	});
	m_farm.onMinerRestart([&]() {
        setThreadName("main");
		cnote << "Restart miners...";
		if (m_farm.isMining()) {
			cnote << "Shutting down miners...";
			m_farm.stop();
		}
        auto vEngineModes = getEngineModes(m_minerType);
        m_farm.start(vEngineModes);
	});
}

void PoolManager::stop()
{
    if (m_running.load(std::memory_order_relaxed)) {
        cnote << "Shutting down...";
        m_running.store(false, std::memory_order_relaxed);
        m_failovertimer.cancel();

        if (p_client->isConnected()) {
            p_client->disconnect();
        }
        if (m_farm.isMining()) {
            cnote << "Shutting down miners...";
            m_farm.stop();
        }
    }
}

void PoolManager::trun()
{
    setThreadName("main");
    while (m_running.load(std::memory_order_relaxed)) {
        // Take action only if not pending state (connecting/disconnecting)
        // Otherwise do nothing and wait until connection state is NOT pending
        if (!p_client->isPendingState()) {
            if (!p_client->isConnected()) {
                // If this connection is marked Unrecoverable then discard it
                if (m_connections[m_activeConnectionIdx].IsUnrecoverable()) {
                    m_connections.erase(m_connections.begin() + m_activeConnectionIdx);
                    m_connectionAttempt = 0;
                    if (m_activeConnectionIdx > 0) {
                        m_activeConnectionIdx--;
                    }
                }
                // Rotate connections if above max attempts threshold
                if (m_connectionAttempt >= m_maxConnectionAttempts) {
                    m_connectionAttempt = 0;
                    m_activeConnectionIdx++;
                    if (m_activeConnectionIdx == m_connections.size()) {
                        m_activeConnectionIdx = 0;
                    }

                    // Suspend mining if applicable as we're switching
                    if (m_farm.isMining()) {
                        cnote << "Suspend mining due connection change...";
                        m_farm.setWork({});//
                    }
                }
                if (m_connections[m_activeConnectionIdx].Host() != "exit"  && m_connections.size() > 0) {
                    // Count connectionAttempts
                    m_connectionAttempt++;

                    // Invoke connections
                    p_client->setConnection(m_connections[m_activeConnectionIdx]);
                    m_farm.set_pool_addresses(m_connections[m_activeConnectionIdx].Host(), m_connections[m_activeConnectionIdx].Port());
                    cnote << "Selected pool" << (m_connections[m_activeConnectionIdx].Host() + ":" + toString(m_connections[m_activeConnectionIdx].Port()));
                    p_client->connect();

                } else {
                    cnote << "No more connections to try. Exiting ...";
                    // Stop mining if applicable
                    if (m_farm.isMining()) {
                        cnote << "Shutting down miners...";
                        m_farm.stop();
                    }

                    m_running.store(false, std::memory_order_relaxed);
                    continue;
                }
            }

        }

        // Hashrate reporting
        m_hashrateReportingTimePassed++;

        if (m_hashrateReportingTimePassed > m_hashrateReportingTime) {
            //auto mp = m_farm.miningProgress();
            //std::string h = toHex(toCompactBigEndian(uint64_t(mp.hashRate), 1));
            //std::string res = h[0] != '0' ? h : h.substr(1);

            // Should be 32 bytes
            // https://github.com/ethereum/wiki/wiki/JSON-RPC#eth_submithashrate
            //std::ostringstream ss;
            //ss << std::setw(64) << std::setfill('0') << res;
            //cnote << res.str();
            //!TODO p_client->submitHashrate();
            m_hashrateReportingTimePassed = 0;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void PoolManager::addConnection(URI &conn)
{
	m_connections.push_back(conn);
}

void PoolManager::clearConnections()
{
    m_connections.clear();
    m_farm.set_pool_addresses("", 0);
    if (p_client && p_client->isConnected()) {
        p_client->disconnect();
    }
}

bool PoolManager::start()
{
    if (m_connections.size() > 0) {
        m_running.store (true, std::memory_order_relaxed);
        m_workThread = std::thread{ boost::bind(&PoolManager::trun, this) };
        // Try to connect to pool
        return true;
    } else {
        cwarn << "Manager has no connections defined!";
        return false;
    }
    return true;
}

void PoolManager::check_failover_timeout(const boost::system::error_code& ec)
{

    if (!ec) {
        if (m_running.load(std::memory_order_relaxed)) {
            if (m_activeConnectionIdx != 0) {
                p_client->disconnect();
                m_activeConnectionIdx = 0;
                m_connectionAttempt = 0;
            }
        }
    }
}

