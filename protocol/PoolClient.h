#pragma once

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <queue>

#include <nrgcore/mineplant.h>
#include <nrgcore/miner.h>
#include "PoolURI.h"

class PoolClient
{
public:
    void setConnection(URI &conn)
    {
        m_conn = conn;
        m_connection_changed = true;
    }

    virtual void connect() = 0;
    virtual void disconnect() = 0;

    virtual void submitHashrate(const std::string& rate) = 0;
    virtual void submitSolution(const energi::Solution& solution) = 0;
    virtual bool isConnected() = 0;
    virtual std::string ActiveEndPoint() = 0;

    using SolutionAccepted = std::function<void(bool const&)>;
    using SolutionRejected = std::function<void(bool const&)>;
    using Disconnected = std::function<void()>;
    using Connected = std::function<void()>;
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

protected:
    bool m_authorized = false;
    bool m_connected = false;
    bool m_connection_changed = false;
    boost::asio::ip::tcp::endpoint m_endpoint;

    URI m_conn;

    SolutionAccepted m_onSolutionAccepted;
    SolutionRejected m_onSolutionRejected;
    Disconnected m_onDisconnected;
    Connected m_onConnected;
    WorkReceived m_onWorkReceived;
};
