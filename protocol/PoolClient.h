#pragma once

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <queue>
#include <chrono>

#include <nrgcore/mineplant.h>
#include <nrgcore/miner.h>
#include "PoolURI.h"

class PoolClient
{
public:
    virtual ~PoolClient() noexcept = default;

    void setConnection(URI &conn)
    {
        m_conn = &conn;
        m_canconnect.store(false, std::memory_order_relaxed);
    }

    virtual void connect() = 0;
    virtual void disconnect() = 0;

    virtual void submitHashrate(const std::string& rate) = 0;
    virtual void submitSolution(const energi::Solution& solution) = 0;
    virtual bool isConnected() = 0;
    virtual bool isPendingState() = 0;
    virtual std::string ActiveEndPoint() = 0;

    using SolutionAccepted = std::function<void(bool const&, const std::chrono::milliseconds&)>;
    using SolutionRejected = std::function<void(bool const&, const std::chrono::milliseconds&)>;
    using Disconnected = std::function<void()>;
    using Connected = std::function<void()>;
    using ResetWork = std::function<void()>;
    using WorkReceived = std::function<void(energi::Work const&)>;

    void onSolutionAccepted(const SolutionAccepted& handler)
    {
        m_onSolutionAccepted = handler;
    }

    void onSolutionRejected(const SolutionRejected& handler)
    {
        m_onSolutionRejected = handler;
    }

    void onDisconnected(const Disconnected& handler)
    {
        m_onDisconnected = handler;
    }

    void onConnected(const Connected& handler)
    {
        m_onConnected = handler;
    }

    void onWorkReceived(const WorkReceived& handler)
    {
        m_onWorkReceived = handler;
    }

    void onResetWork(const ResetWork& handler)
    {
        m_onResetWork = handler;
    }

protected:
    std::atomic<bool> m_subscribed = { false };
    std::atomic<bool> m_authorized = { false };
    std::atomic<bool> m_connected = { false };
    std::atomic<bool> m_canconnect = { false };

    boost::asio::ip::basic_endpoint<boost::asio::ip::tcp> m_endpoint;

    URI* m_conn = nullptr;

    SolutionAccepted m_onSolutionAccepted;
    SolutionRejected m_onSolutionRejected;
    Disconnected m_onDisconnected;
    Connected m_onConnected;
    ResetWork m_onResetWork;
    WorkReceived m_onWorkReceived;
};
