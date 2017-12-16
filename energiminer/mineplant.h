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


namespace energi
{
  enum class EnumMinerEngine : unsigned
  {
    kCPU    = 0x0,
    kCL     = 0x1,
    kTest   = 0x2
  };

  constexpr const char* StrMinerEngine[] = { "CPU", "CL", "Test" };

  inline std::string to_string(EnumMinerEngine minerEngine)
  {
    return StrMinerEngine[static_cast<int>(minerEngine)];
  }


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
    MinePlant(SolutionFoundCallback& solution_found_cb):solution_found_cb_(solution_found_cb)
    {

    }
    ~MinePlant()
    {
      stop();
    }

    bool start(const std::vector<EnumMinerEngine> &vMinerEngine);
    void stop();

    inline bool isStarted(){ return started_; }

    bool setWork(const Work& work);
    void submit(const Solution &sol) const override;
    WorkingProgress const& miningProgress() const;

  private:
    void collectHashRate();

    bool                    started_   = false;
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

  };

} /* namespace energi */

#endif /* ENERGIMINER_MINEPLANT_H_ */
