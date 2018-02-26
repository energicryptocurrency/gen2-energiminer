#include "energiminer/mineplant.h"
#include "StratumClient.h"
#include "energiminer/Log.h"
#include "BuildInfo.h"

using boost::asio::ip::tcp;

StratumClient::StratumClient(energi::MinePlant* f,
                             MinerExecutionMode mode,
                             const std::string& farmURL,
                             const std::string& user,
                             const std::string& pass,
                             unsigned maxretries,
                             int timeout)
	: m_minerType(mode)
    , m_socket(m_io_service)
    , m_worktimer(m_io_service)

{
    std::size_t portPos = farmURL.find_last_of(":");

    m_primary.host = farmURL.substr(0, portPos);
    m_primary.port = farmURL.substr(portPos + 1);
    m_primary.user = user;
    m_primary.pass = pass;

    p_active = &m_primary;
    m_authorized = false;
    m_connected = false;
    m_maxRetries = maxretries;
    m_worktimeout = timeout;

    p_farm = f;
    connect();
}

void StratumClient::processExtranonce(std::string& enonce)
{
    m_extraNonceHexSize = enonce.length();
    cnote << "Extranonce set to " << enonce;
    for (int i = enonce.length(); i < 16; ++i) {
        enonce += "0";
    }
}

StratumClient::~StratumClient()
{
}

void StratumClient::setFailover(const std::string& failOverURL)
{

    std::size_t portPos = failOverURL.find_last_of(":");
    std::size_t passPos = failOverURL.find_last_of(":", portPos - 1);
    std::size_t atPos   = failOverURL.find_first_of("@");

	m_failover.host = failOverURL.substr(atPos + 1, portPos - atPos - 1);
	m_failover.port = failOverURL.substr(portPos + 1);
	m_failover.user = failOverURL.substr(7, passPos - 7);
	m_failover.pass = failOverURL.substr(passPos + 1, atPos - passPos - 1);
}

bool StratumClient::submit(const energi::Solution& solution)
{
    std::string solutionNonce = std::to_string(solution.m_nonce);
    std::string minernonce;
    std::string nonceHex = energi::GetHex(reinterpret_cast<uint8_t*>(&solutionNonce[0]), solutionNonce.size());
    minernonce = nonceHex.substr(m_extraNonceHexSize, 16 - m_extraNonceHexSize);
    std::ostream os(&m_requestBuffer);
    std::string json = "{\"id\": 4, \"method\": \"mining.submit\", \"params\": [\"" + p_active->user + "\",\"" + solution.getJobName() + "\",\"" + minernonce + "\", \"82345678\"]}\n";
    cnote << json;
    os << json;
    write(m_socket, m_requestBuffer);
    cnote << "Solution found; Submitted to" << p_active->host;
    return true;
}

void StratumClient::reconnect()
{
    m_worktimer.cancel();
    m_authorized = false;
    m_connected = false;

    if (!m_failover.host.empty()) {
        m_retries++;
        if (m_retries > m_maxRetries) {
            if (m_failover.host == "exit") {
                disconnect();
                return;
            } else if (p_active == &m_primary) {
                p_active = &m_failover;
            } else {
                p_active = &m_primary;
            }
            m_retries = 0;
        }
    }
    cnote << "Reconnecting in 3 seconds...";
    boost::asio::deadline_timer timer(m_io_service, boost::posix_time::seconds(3));
    timer.wait();
}

void StratumClient::connect()
{
    cnote << "Connecting to stratum server " << p_active->host + ":" + p_active->port;
    tcp::resolver r(m_io_service);
    tcp::resolver::query q(p_active->host, p_active->port);
    tcp::resolver::iterator endpoint_iterator = r.resolve(q);
    tcp::resolver::iterator end;

    boost::system::error_code error = boost::asio::error::host_not_found;
    while (error && endpoint_iterator != end) {
        m_socket.close();
        m_socket.connect(*endpoint_iterator++, error);
    }
    if (error) {
        std::cerr << "Could not connect to stratum server " << p_active->host + ":" + p_active->port + ", " << error.message();
        reconnect();
    } else {
        cnote << "Connected!";
        m_connected = true;
        if (!p_farm->isStarted()) {
            cnote << "Starting farm";
            const auto modes = getEngineModes(m_minerType);
            p_farm->start(modes);
        }
        std::ostream os(&m_requestBuffer);
        os << "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"energiminer/" << ENERGI_PROJECT_VERSION << "\",\"EnergiStratum/1.0.0\"]}\n";
        write(m_socket, m_requestBuffer);
    }
}

void StratumClient::disconnect()
{
    cdebug << "Disconnecting";
    m_connected = false;
    m_running = false;
    if (p_farm->isStarted()) {
        cnote << "Stopping farm";
        p_farm->stop();
    }
    m_socket.close();
}

void StratumClient::workTimeoutHandler(const boost::system::error_code& ec)
{
    if (!ec) {
        cnote << "No new work received in" << m_worktimeout << "seconds.";
        reconnect();
    }
}

bool StratumClient::processResponse(Json::Value& responseObject)
{
    Json::Value error = responseObject.get("error", new Json::Value);
    if (error.isArray()) {
        std::string msg = error.get(1, "Unknown error").asString();
        cnote << msg;
    }
    std::ostream os(&m_requestBuffer);
    Json::Value params;
    int id = responseObject.get("id", Json::Value::null).asInt();
    switch (id) {
        case 1:
            m_nextWorkDifficulty = 1;
            params = responseObject.get("result", Json::Value::null);
            if (params.isArray()) {
                std::string enonce = params.get((Json::Value::ArrayIndex)1, "").asString();
                processExtranonce(enonce);
            }
            os << "{\"id\": 2, \"method\": \"mining.extranonce.subscribe\", \"params\": []}\n";
			m_authorized = true;
            cnote << "Subscribed to stratum server";
            os << "{\"id\": 3, \"method\": \"mining.authorize\", \"params\": [\"" << p_active->user << "\",\"" << p_active->pass << "\"]}\n";

            // not strictly required but it does speed up initialization
			write(m_socket, m_requestBuffer);
            break;
        default:
            std::string method, workattr;
            method = responseObject.get("method", "").asString();
            workattr = "params";
            if (method == "mining.notify") {
                params = responseObject.get(workattr, Json::Value::null);
                if (params.isArray()) {
                    auto workGBT = params.get((Json::Value::ArrayIndex)0, "");
                    const auto& master = workGBT["masternode"];
                    std::string job = params.get((Json::Value::ArrayIndex)1, "").asString();
                    m_current = energi::Work(workGBT, master["payee"].asString(), job);
                    m_worktimer.cancel();
                    m_worktimer.expires_from_now(boost::posix_time::seconds(m_worktimeout));
                    m_worktimer.async_wait(boost::bind(&StratumClient::workTimeoutHandler, this, boost::asio::placeholders::error));
                    return true;
                }
            } else if (method == "mining.set_difficulty") {
                params = responseObject.get("params", Json::Value::null);
                if (params.isArray()) {
                    m_nextWorkDifficulty = params.get((Json::Value::ArrayIndex)0, 1).asDouble();
                    if (m_nextWorkDifficulty <= 0.0001) {
                        m_nextWorkDifficulty = 0.0001;
                    }
                    cnote << "Difficulty set to " << m_nextWorkDifficulty;
                }
            } else if (method == "mining.set_extranonce") {
                params = responseObject.get("params", Json::Value::null);
                if (params.isArray()) {
                    std::string enonce = params.get((Json::Value::ArrayIndex)0, "").asString();
                    processExtranonce(enonce);
                }
            } else if (method == "client.get_version") {
                os << "{\"error\": null, \"id\" : " << id << ", \"result\" : \"" << ENERGI_PROJECT_VERSION << "\"}\n";
                write(m_socket, m_requestBuffer);
            }
            break;
    }
    return false;
}

Json::Value StratumClient::getBlockTemplate() throw (jsonrpc::JsonRpcException)
{
    bool process = false;
    while (m_running && !process) {
        try {
            if (!m_connected) {
                connect();
            }
            read_until(m_socket, m_responseBuffer, "\n");
            std::istream is(&m_responseBuffer);
            std::string response;
            getline(is, response);
            if (!response.empty() && response.front() == '{' && response.back() == '}') {
                Json::Value responseObject;
                Json::Reader reader;
                if (reader.parse(response.c_str(), responseObject)) {
                    process = processResponse(responseObject);
                } else {
                    cwarn << "Parse response failed: " << reader.getFormattedErrorMessages();
                }
            } else {
                cwarn << "Discarding incomplete response";
            }
        } catch (const std::exception& ex) {
            cwarn << ex.what();
            reconnect();
        }
    }
    return Json::Value();
}

energi::Work StratumClient::getWork()
{
    getBlockTemplate();
    return m_current;
}
