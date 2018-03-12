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


namespace energi
{
  class SolutionStats {
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
        nonce_scumbler_ = std::uniform_int_distribution<uint64_t>()(engine);
    }
    ~MinePlant()
    {
      stop();
    }

    bool start(const std::vector<EnumMinerEngine> &vMinerEngine);
    void stop();

    uint64_t get_nonce_scumbler() const override
    {
        return nonce_scumbler_;
    }

    inline bool isStarted(){ return started_; }
    //bool solutionFound() const override;

    bool setWork(const Work& work);
    void stopAllWork();
    void submit(const Solution &sol) const override;
    WorkingProgress const& miningProgress() const;

  private:
    void collectHashRate();

    bool                    started_   = false;
    mutable std::atomic<bool>       solutionFound_;
    SolutionFoundCallback   solution_found_cb_;
    Miners                  miners_;
    Work                    work_;
    mutable std::mutex      mutex_work_;

    std::chrono::steady_clock::time_point lastStart_;
    std::vector<WorkingProgress>          lastProgresses_;
    SolutionStats                         solutionStats_;
    int                                   hashrateSmoothInterval_ = 10000;
    std::unique_ptr<std::thread>          tHashrateTimer_;
    bool                                  continueHashTimer_ = true;

    mutable std::mutex                    mutex_progress_;
    mutable WorkingProgress               progress_;
    uint64_t                              nonce_scumbler_;

  };

} /* namespace energi */

#endif /* ENERGIMINER_MINEPLANT_H_ */
