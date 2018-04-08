/*
 * MinePlant.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_MINEPLANT_H_
#define ENERGIMINER_MINEPLANT_H_

#include "energiminer/plant.h"
#include "energiminer/miner.h"
#include "energiminer/solution.h"


#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <chrono>
#include <random>


namespace energi {

class SolutionStats
{
public:
    void accepted() { accepts++;  }
    void rejected() { rejects++;  }
    void failed()   { failures++; }

    void acceptedStale() { acceptedStales++; }
    void rejectedStale() { rejectedStales++; }


    void reset() { accepts = rejects = failures = acceptedStales = rejectedStales = 0; }

    unsigned getAccepts()     { return accepts; }
    unsigned getRejects()     { return rejects; }
    unsigned getFailures()      { return failures; }
    unsigned getAcceptedStales()  { return acceptedStales; }
    unsigned getRejectedStales()  { return rejectedStales; }
private:
    unsigned accepts  = 0;
    unsigned rejects  = 0;
    unsigned failures = 0;

    unsigned acceptedStales = 0;
    unsigned rejectedStales = 0;
};

inline std::ostream& operator<<(std::ostream& os, SolutionStats s)
{
    return os << "[A" << s.getAccepts() << "+" << s.getAcceptedStales() << ":R" << s.getRejects() << "+" << s.getRejectedStales() << ":F" << s.getFailures() << "]";
}


class MinePlant : public Plant
{
public:
    MinePlant(SolutionFoundCallback& solution_found_cb)
        :solution_found_cb_(solution_found_cb)
    {
        std::random_device engine;
        m_nonceScumbler = std::uniform_int_distribution<uint64_t>()(engine);
    }
    ~MinePlant()
    {
        stop();
    }

    bool start(const std::vector<EnumMinerEngine> &vMinerEngine);
    void stop();

    uint64_t get_nonce_scumbler() const override
    {
        return m_nonceScumbler;
    }

    inline bool isStarted(){ return m_started; }
    //bool solutionFound() const override;

    bool setWork(const Work& work);
    void stopAllWork();
    void submit(const Solution &sol) const override;
    const WorkingProgress& miningProgress() const;

private:
    void collectHashRate();

private:
    bool      m_started   = false;
    bool      m_continueHashTimer = true;
    int       m_hashrateSmoothInterval = 10000;
    uint64_t  m_nonceScumbler;

    mutable std::atomic<bool>  m_solutionFound;
    SolutionFoundCallback      solution_found_cb_;
    Miners                     m_miners;
    Work                       m_work;

    mutable std::mutex      m_mutexWork;
    mutable std::mutex      m_mutexProgress;

    std::chrono::steady_clock::time_point m_lastStart;
    SolutionStats                         m_solutionStats;
    std::unique_ptr<std::thread>          m_tHashrateTimer;

    mutable WorkingProgress      m_progress;
    std::vector<WorkingProgress> m_lastProgresses;

};

} //! namespace energi

#endif /* ENERGIMINER_MINEPLANT_H_ */
