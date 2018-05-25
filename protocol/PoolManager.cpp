#include "PoolManager.h"
#include <chrono>

using namespace energi;

PoolManager::PoolManager(PoolClient* client,
                         energi::MinePlant &farm,
                         const MinerExecutionMode& minerType)
    : Worker("main")
    , m_farm(farm)
    , m_minerType(minerType)
{
	p_client = client;

	p_client->onConnected([&]()
	{
        cnote << "Connected to " << m_connections[m_activeConnectionIdx].Host() << p_client->ActiveEndPoint();
        if (!m_farm.isMining()) {
            cnote << "Spinning up miners...";
            auto vEngineModes = getEngineModes(m_minerType);
            m_farm.start(vEngineModes);
    }
	});
	p_client->onDisconnected([&]()
	{
		cnote << "Disconnected from " + m_connections[m_activeConnectionIdx].Host() << p_client->ActiveEndPoint();
		if (m_farm.isMining()) {
			cnote << "Shutting down miners...";
			m_farm.stop();
		}
		if (m_running) {
			tryReconnect();
        }
	});
	p_client->onWorkReceived([&](const Work& wp)
	{
		m_reconnectTry = 0;
		m_farm.setWork(wp);
	});
	p_client->onSolutionAccepted([&](const bool& stale)
	{
		using namespace std::chrono;
		auto ms = duration_cast<milliseconds>(steady_clock::now() - m_submit_time);
		std::stringstream ss;
		ss << std::setw(4) << std::setfill(' ') << ms.count();
		ss << "ms." << "   " << m_connections[m_activeConnectionIdx].Host() + p_client->ActiveEndPoint();
		cnote << EthLime "**Accepted" EthReset << (stale ? "(stale)" : "") << ss.str();
		m_farm.acceptedSolution(stale);
	});
	p_client->onSolutionRejected([&](const bool& stale)
	{
		using namespace std::chrono;
		auto ms = duration_cast<milliseconds>(steady_clock::now() - m_submit_time);
		std::stringstream ss;
		ss << std::setw(4) << std::setfill(' ') << ms.count();
		ss << "ms." << "   " << m_connections[m_activeConnectionIdx].Host() + p_client->ActiveEndPoint();
		cwarn << EthRed "**Rejected" EthReset << (stale ? "(stale)" : "") << ss.str();
		m_farm.rejectedSolution(stale);
	});

	m_farm.onSolutionFound([&](Solution sol)
	{
        // Solution should passthrough only if client is
        // properly connected. Otherwise we'll have the bad behavior
        // to log nonce submission but receive no response
        if (p_client->isConnected()) {
            m_submit_time = std::chrono::steady_clock::now();
            p_client->submitSolution(sol);
        } else {
            cnote << std::string(EthRed "Nonce ") + std::to_string(sol.getNonce()) << " wasted. Waiting for connection ...";
        }
    return false;
	});
	m_farm.onMinerRestart([&]() {
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
    if (m_running) {
        cnote << "Shutting down...";
        m_running = false;
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
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        m_hashrateReportingTimePassed++;
        // Hashrate reporting
        if (m_hashrateReportingTimePassed > m_hashrateReportingTime) {
            auto mp = m_farm.miningProgress();
            mp.rate();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            cnote << mp;
            m_hashrateReportingTimePassed = 0;
        }
    }
}

void PoolManager::addConnection(URI &conn)
{
	m_connections.push_back(conn);
	if (m_connections.size() == 1) {
		p_client->setConnection(conn);
		m_farm.set_pool_addresses(conn.Host(), conn.Port());
	}
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
        m_running = true;
        startWorking();
        // Try to connect to pool
        cnote << "Selected pool" << (m_connections[m_activeConnectionIdx].Host() + ":" + toString(m_connections[m_activeConnectionIdx].Port()));
        p_client->connect();
        return true;
    } else {
        cwarn << "Manager has no connections defined!";
        return false;
    }
    return true;
}

void PoolManager::tryReconnect()
{
    // No connections available, so why bother trying to reconnect
    if (m_connections.size() <= 0) {
        cwarn << "Manager has no connections defined!";
        return;
    }
    for (auto i = 4; --i; std::this_thread::sleep_for(std::chrono::seconds(1))) {
        cnote << "Retrying in " << i << "... \r";
    }
    // We do not need awesome logic here, we just have one connection anyway
    if (m_connections.size() == 1) {
        cnote << "Selected pool" << (m_connections[m_activeConnectionIdx].Host() + ":" + toString(m_connections[m_activeConnectionIdx].Port()));
        p_client->connect();
        return;
    }
    // Fallback logic, tries current connection multiple times and then switches to
    // one of the other connections.
    if (m_reconnectTries > m_reconnectTry) {
        m_reconnectTry++;
        cnote << "Selected pool" << (m_connections[m_activeConnectionIdx].Host() + ":" + toString(m_connections[m_activeConnectionIdx].Port()));
        p_client->connect();
    } else {
        m_reconnectTry = 0;
        m_activeConnectionIdx++;
        if (m_activeConnectionIdx >= m_connections.size()) {
            m_activeConnectionIdx = 0;
        }
        if (m_connections[m_activeConnectionIdx].Host() == "exit") {
            cnote << "Exiting because reconnecting is not possible.";
            stop();
        }
        else {
            p_client->setConnection(m_connections[m_activeConnectionIdx]);
            m_farm.set_pool_addresses(m_connections[m_activeConnectionIdx].Host(), m_connections[m_activeConnectionIdx].Port());
            cnote << "Selected pool" << (m_connections[m_activeConnectionIdx].Host() + ":" + toString(m_connections[m_activeConnectionIdx].Port()));
            p_client->connect();
        }
    }
}
