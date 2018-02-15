#include "energiminer/mineplant.h"
#include "StratumClient.h"
#include "energiminer/Log.h"

using boost::asio::ip::tcp;

StratumClient::StratumClient(energi::MinePlant* f,
                                       MinerExecutionMode mode,
                                       const std::string& farmURL,
                                       unsigned maxretries,
                                       int timeout)
	: energi::Worker("stratum")
    , m_minerType(mode)
    , m_current(to_string(getEngineMode(mode)), *f, f->get_nonce_scumbler())
    , m_socket(m_io_service)
    , m_worktimer(m_io_service)

{
    std::size_t portPos = farmURL.find_last_of(":");
    std::size_t passPos = farmURL.find_last_of(":", portPos - 1);
    std::size_t atPos = farmURL.find_first_of("@");

	m_minerType = mode;
	m_primary.host = farmURL.substr(atPos + 1, portPos - atPos - 1);
	m_primary.port = farmURL.substr(portPos + 1);
	m_primary.user = farmURL.substr(7, passPos - 7);
	m_primary.pass = farmURL.substr(passPos + 1, atPos - passPos - 1);

	p_active = &m_primary;
	m_authorized = false;
	m_connected = false;
	m_maxRetries = maxretries;
	m_worktimeout = timeout;

	p_farm = f;
	start();
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

//! TODO remove
bool StratumClient::submitHashrate(const std::string& rate)
{
	return true;
}

bool StratumClient::submit(const energi::Solution& solution)
{
    std::string blockData = solution.getSubmitBlockData();
    std::string json = "{\"jsonrpc\":\1.0\", \"id\": 4, \"method\": \"submitblock\", \"params\": [\"" + blockData + "\"]}\n";
    std::ostream os(&m_requestBuffer);
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

        std::string user;
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

void StratumClient::work_timeout_handler(const boost::system::error_code& ec)
{
    if (!ec) {
        cnote << "No new work received in" << m_worktimeout << "seconds.";
        reconnect();
    }
}

void StratumClient::processExtranonce(std::string& enonce)
{
    m_extraNonceHexSize = enonce.length();
    cnote << "Extranonce set to " << enonce;
    for (int i = enonce.length(); i < 16; ++i)
        enonce += "0";
    //m_extraNonce = h64(enonce);
}

void StratumClient::processReponse(Json::Value& responseObject)
{
//    Json::Value error = responseObject.get("error", new Json::Value);
//    if (error.isArray()) {
//        std::string msg = error.get(1, "Unknown error").asString();
//        cnote << msg;
//    }
//    std::ostream os(&m_requestBuffer);
//    Json::Value params;
//    int id = responseObject.get("id", Json::Value::null).asInt();
//    switch (id)
//    {
//        case 1:
//            m_authorized = true;
//            os << "{\"id\": 5, \"method\": \"getblocktemplate\", \"params\": []}\n";
//            write(m_socket, m_requestBuffer);
//            break;
//        case 2:
//            // nothing to do...
//            break;
//        case 3:
//            m_authorized = responseObject.get("result", Json::Value::null).asBool();
//            if (!m_authorized) {
//                disconnect();
//                return;
//            }
//            break;
//        case 4:
//            if (responseObject.get("result", false).asBool()) {
//                //p_farm->acceptedSolution(m_stale);
//            }
//            else {
//                //cwarn << "Rejected.";
//                //p_farm->rejectedSolution(m_stale);
//            }
//            break;
//        default:
//            std::string method, workattr;
//            unsigned index;
//
//            method = responseObject.get("method", "").asString();
//            workattr = "params";
//            index = 1;
//
//            if (method == "getblockcount") {
//                params = responseObject.get(workattr, Json::Value::null);
//                if (params.isArray()) {
//                    std::string job = params.get((Json::Value::ArrayIndex)0, "").asString();
//                    if (m_protocol == STRATUM_PROTOCOL_ETHEREUMSTRATUM) {
//                        std::string job = params.get((Json::Value::ArrayIndex)0, "").asString();
//                        std::string sSeedHash = params.get((Json::Value::ArrayIndex)1, "").asString();
//                        std::string sHeaderHash = params.get((Json::Value::ArrayIndex)2, "").asString();
//
//                        if (sHeaderHash != "" && sSeedHash != "") {
//                            m_worktimer.cancel();
//                            m_worktimer.expires_from_now(boost::posix_time::seconds(m_worktimeout));
//                            m_worktimer.async_wait(boost::bind(&EthStratumClientV2::work_timeout_handler, this, boost::asio::placeholders::error));
//
//                            m_current.header = h256(sHeaderHash);
//                            m_current.seed = h256(sSeedHash);
//                            m_current.boundary = h256();
//                            diffToTarget((uint32_t*)m_current.boundary.data(), m_nextWorkDifficulty);
//                            m_current.startNonce = ethash_swap_u64(*((uint64_t*)m_extraNonce.data()));
//                            m_current.exSizeBits = m_extraNonceHexSize * 4;
//                            m_current.job = h256(job);
//
//                            p_farm->setWork(m_current);
//                            cnote << "Received new job #" + job.substr(0, 8)
//                                << " seed: " << "#" + m_current.seed.hex().substr(0, 32)
//                                << " target: " << "#" + m_current.boundary.hex().substr(0, 24);
//                        }
//                    } else {
//                        string sHeaderHash = params.get((Json::Value::ArrayIndex)index++, "").asString();
//                        string sSeedHash = params.get((Json::Value::ArrayIndex)index++, "").asString();
//                        string sShareTarget = params.get((Json::Value::ArrayIndex)index++, "").asString();
//
//                        // coinmine.pl fix
//                        int l = sShareTarget.length();
//                        if (l < 66)
//                            sShareTarget = "0x" + string(66 - l, '0') + sShareTarget.substr(2);
//
//
//                        if (sHeaderHash != "" && sSeedHash != "" && sShareTarget != "") {
//                            m_worktimer.cancel();
//                            m_worktimer.expires_from_now(boost::posix_time::seconds(m_worktimeout));
//                            m_worktimer.async_wait(boost::bind(&EthStratumClientV2::work_timeout_handler, this, boost::asio::placeholders::error));
//
//                            h256 headerHash = h256(sHeaderHash);
//
//                            if (headerHash != m_current.header) {
//                                m_current.header = h256(sHeaderHash);
//                                m_current.seed = h256(sSeedHash);
//                                m_current.boundary = h256(sShareTarget);
//                                m_current.job = h256(job);
//
//                                p_farm->setWork(m_current);
//                            }
//                            cnote << "Received new job #" + job.substr(0, 8)
//                                << " seed: " << "#" + m_current.seed.hex().substr(0, 32)
//                                << " target: " << "#" + m_current.boundary.hex().substr(0, 24);
//                        }
//                    }
//                }
//            } else if (method == "mining.set_difficulty" && m_protocol == STRATUM_PROTOCOL_ETHEREUMSTRATUM) {
//                params = responseObject.get("params", Json::Value::null);
//                if (params.isArray()) {
//                    m_nextWorkDifficulty = params.get((Json::Value::ArrayIndex)0, 1).asDouble();
//                    if (m_nextWorkDifficulty <= 0.0001) m_nextWorkDifficulty = 0.0001;
//                    cnote << "Difficulty set to " << m_nextWorkDifficulty;
//                }
//            } else if (method == "mining.set_extranonce" && m_protocol == STRATUM_PROTOCOL_ETHEREUMSTRATUM) {
//                params = responseObject.get("params", Json::Value::null);
//                if (params.isArray()) {
//                    std::string enonce = params.get((Json::Value::ArrayIndex)0, "").asString();
//                    processExtranonce(enonce);
//                }
//            } else if (method == "client.get_version") {
//                os << "{\"error\": null, \"id\" : " << id << ", \"result\" : \"" << ETH_PROJECT_VERSION << "\"}\n";
//                write(m_socket, m_requestBuffer);
//            }
//            break;
//    }
}

void StratumClient::trun()
{
    std::ostream os(&m_requestBuffer);
    while (m_running) {
        try {
            if (!m_connected) {
                connect();
            }
            read_until(m_socket, m_responseBuffer, "\n");
            std::istream is(&m_responseBuffer);
            std::string response;
            getline(is, response);

            if (!response.empty() && response.front() == '{' && response.back() == '}') {
                m_authorized = true;
                os << "{\"id\": 5, \"method\": \"getblocktemplate\", \"params\": []}\n";
                write(m_socket, m_requestBuffer);
                Json::Value responseObject;
                Json::Reader reader;
                if (reader.parse(response.c_str(), responseObject)) {
                    std::cout << responseObject << std::endl;
                    processReponse(responseObject);
                }
                //    m_response = response;
                //} else {
                //    cwarn << "Parse response failed: " << reader.getFormattedErrorMessages();
                //}
            } else {
                cnote << "Discarding incomplete response";
            }
        } catch (const std::exception& _e) {
            cwarn << _e.what();
            reconnect();
        }
    }
}
