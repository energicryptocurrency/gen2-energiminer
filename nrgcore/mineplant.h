/*
 * MinePlant.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_MINEPLANT_H_
#define ENERGIMINER_MINEPLANT_H_

#include "plant.h"
#include "miner.h"
#include "primitives/solution.h"
#include <boost/asio.hpp>


#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <chrono>
#include <random>
#include <libhwmon/wrapnvml.h>
#include <libhwmon/wrapadl.h>
#if defined(__linux)
#include <libhwmon/wrapamdsysfs.h>
#endif


namespace energi {

class SolutionStats
{
public:
    void accepted() { accepts++;  }
    void rejected() { rejects++;  }
    void failed()   { failures++; }

    void acceptedStale() { acceptedStales++; }


    void reset() { accepts = rejects = failures = acceptedStales = 0; }

    unsigned getAccepts()     { return accepts; }
    unsigned getRejects()     { return rejects; }
    unsigned getFailures()      { return failures; }
    unsigned getAcceptedStales()  { return acceptedStales; }
private:
    unsigned accepts  = 0;
    unsigned rejects  = 0;
    unsigned failures = 0;

    unsigned acceptedStales = 0;
};

inline std::ostream& operator<<(std::ostream& os, SolutionStats s)
{
    os << "[A" << s.getAccepts();
    if (s.getAcceptedStales()) {
        os << "+" << s.getAcceptedStales();
    }
    if (s.getRejects()) {
        os << ":R" << s.getRejects();
    }
    if (s.getFailures()) {
        os << ":F" << s.getFailures();
    }
    return os << "]";
}


class MinePlant : public Plant
{
public:
    MinePlant(boost::asio::io_service & io_service);
    ~MinePlant();

    bool start(const std::vector<EnumMinerEngine> &vMinerEngine);
    void stop();

    uint64_t get_start_nonce(const Work& work, unsigned idx) const override;
    uint64_t get_nonce_scumbler() const override;
    //! Temperature
    void setTStartTStop(unsigned tstart, unsigned tstop);
    unsigned get_tstart() const override
    {
        return m_tstart;
    }

    unsigned get_tstop() const override
    {
        return m_tstop;
    }

    void set_pool_addresses(const std::string& host, unsigned port);
    const std::string& get_pool_addresses() const;

    //bool solutionFound() const override;

    void setWork(const Work& work);
    void submitProof(const Solution &sol) const override;
    const WorkingProgress& miningProgress(bool hwmon = false, bool power = false) const;

    using SolutionFound = std::function<void(Solution const&)>;
    using MinerRestart = std::function<void()>;

    /**
     * @brief Provides a valid header based upon that received previously with setWork().
     * @param _bi The now-valid header.
     * @return true if the header was good and that the Farm should pause until more work is submitted.
     */
    void onSolutionFound(const SolutionFound& handler)
    {
        m_onSolutionFound = handler;
    }
    void onMinerRestart(const MinerRestart& handler)
    {
        m_onMinerRestart = handler;
    }

	void processHashRate(const boost::system::error_code& ec);
	/**
	 * @brief Stop all mining activities and Starts them again
	 */
	void restart();
	bool isMining() const;
	SolutionStats getSolutionStats();
	void failedSolution() override;
	void acceptedSolution(bool _stale);
	void rejectedSolution();
    const Work& getWork() const;
	std::chrono::steady_clock::time_point farmLaunched();
    std::string farmLaunchedFormatted() const;

private:
    void collectHashRate();

private:
	mutable std::mutex                  x_minerWork;
	Miners                              m_miners;
	Work                                m_work;

	std::atomic<bool>                   m_isMining = {false};

	mutable WorkingProgress             m_progress;

	SolutionFound                       m_onSolutionFound;
	MinerRestart                        m_onMinerRestart;

	//std::map<std::string, SealerDescriptor> m_sealers;
	//std::string                             m_lastSealer;
	bool                                    b_lastMixed = false;

    std::chrono::steady_clock::time_point   m_lastStart;
    uint64_t m_hashrateSmoothInterval = 10000;

    boost::asio::io_service::strand m_io_strand;
    boost::asio::deadline_timer             m_hashrateTimer;
    std::vector<WorkingProgress>            m_lastProgresses;

    SolutionStats                           m_solutionStats;
    std::chrono::steady_clock::time_point   m_farm_launched = std::chrono::steady_clock::now();

    std::string                             m_pool_addresses;
    uint64_t                                m_nonceScumbler;
    unsigned m_tstart = 0;
    unsigned m_tstop = 0;

    wrap_nvml_handle *nvmlh = nullptr;
    wrap_adl_handle *adlh = nullptr;
#if defined(__linux)
    wrap_amdsysfs_handle *sysfsh = nullptr;
#endif
};

} //! namespace energi

#endif /* ENERGIMINER_MINEPLANT_H_ */
