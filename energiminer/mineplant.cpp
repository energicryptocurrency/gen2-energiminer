/*
 * MinePlant.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include "mineplant.h"

#include "libegihash-cl/OpenCLMiner.h"
#include "energiminer/miner.h"
#include "energiminer/work.h"
#include "energiminer/common.h"
#include "energiminer/CpuMiner.h"
#include "energiminer/TestMiner.h"
#include "energiminer/Log.h"

#include <iostream>
#include <limits>

namespace energi
{
  MinerPtr createMiner(EnumMinerEngine minerEngine, int index, const MinePlant &plant)
  {
    switch(minerEngine)
    {
    case EnumMinerEngine::kCL:
      return MinerPtr(new OpenCLMiner(plant, index));
    case EnumMinerEngine::kCPU:
      return MinerPtr(new CpuMiner(plant, index));
    case EnumMinerEngine::kTest:
      return MinerPtr(new TestMiner(plant, index));
    }

    return nullptr;
  }



  bool MinePlant::start(const std::vector<EnumMinerEngine> &vMinerEngine)
  {
    if ( isStarted() )
    {
      return true;
    }

    started_ = true;
    for ( auto &minerEngine : vMinerEngine)
    {
      unsigned int num_threads_for_cpu = std::thread::hardware_concurrency() - 2;
      auto count = EnumMinerEngine::kCL  == minerEngine ? OpenCLMiner::instances() : ( EnumMinerEngine::kTest  == minerEngine ? 2 : num_threads_for_cpu );
      for ( decltype(count) i = 0; i < count; ++i )
      {
        auto miner = createMiner(minerEngine, i, *this);
        started_ &= miner->start();

        if ( started_ )
        {
          miners_.push_back(miner);
        }
      }
    }

    if (tHashrateTimer_)
    {
      std::cerr << "Hash Rate Thread active !, weird" << std::endl;
      return false;
    }
    else
    {
      tHashrateTimer_.reset(new std::thread([&]()
      {
        while( continueHashTimer_ )
        {
          collectHashRate();
          std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
      }));
    }

    // Failure ? return right away
    return started_;
  }


  void MinePlant::stop()
  {
    for ( auto &miner : miners_)
    {
      miner->stop();
    }

    if( tHashrateTimer_ )
    {
      continueHashTimer_ = false;
      tHashrateTimer_->join();
      tHashrateTimer_.reset();
    }

    started_ = false;
  }


  bool MinePlant::setWork(const Work& work)
  {
    //Collect hashrate before miner reset their work
    collectHashRate();

    // if new work hasnt changed, then ignore
    MutexLGuard l(mutex_work_);

    if ( work == work_ )
    {
      return false;
    }

    cdebug << "New Work assigned: Height: " << work.height << " " << work.targetStr;
    work_ = work;

    // Propagate to all miners
    const auto minersCount = miners_.size();
    const auto kLimitPerThread = std::numeric_limits<uint32_t>::max() / minersCount;
    uint32_t index = 0;
    for (auto &miner: miners_)
    {
      auto first  = index * kLimitPerThread;
      auto end    = first + kLimitPerThread;
      cdebug << "first:" << first << "end: " << end;

      miner->setWork(work, first, end);

      ++index;
    }

    return true;
  }


  void MinePlant::submit(const Solution &solution) const
  {
    solution_found_cb_(solution);
  }

  void MinePlant::collectHashRate()
  {
    WorkingProgress p;
    MutexLGuard l(mutex_work_);
    p.ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastStart_).count();

    //Collect
    for (auto const& miner : miners_)
    {
      auto minerHashCount = miner->hashCount();
      p.hashes += minerHashCount;
      p.minersHashes.push_back(minerHashCount);
    }

    //Reset miner hashes
    for (auto const& miner : miners_)
    {
      miner->resetHashCount();
    }

    lastStart_ = std::chrono::steady_clock::now();

    if (p.hashes > 0)
    {
      lastProgresses_.push_back(p);
    }

    // We smooth the hashrate over the last x seconds
    int allMs = 0;
    for (auto const& cp : lastProgresses_) {
      allMs += cp.ms;
    }

    if (allMs > hashrateSmoothInterval_)
    {
      lastProgresses_.erase(lastProgresses_.begin());
    }
  }

  /*
   * @brief Get information on the progress of mining this work.
   * @return The progress with mining so far.
  */
  WorkingProgress const& MinePlant::miningProgress() const
  {
    WorkingProgress p;
    p.ms = 0;
    p.hashes = 0;
    {
      MutexLGuard l(mutex_work_);
      for (auto const& miner : miners_)
      {
        (void) miner;
        p.minersHashes.push_back(0);
      }

      for (auto const& cp : lastProgresses_)
      {
        p.ms += cp.ms;
        p.hashes += cp.hashes;
        for (unsigned int i = 0; i < cp.minersHashes.size(); i++)
        {
          p.minersHashes.at(i) += cp.minersHashes.at(i);
        }
      }
    }

    MutexLGuard l(mutex_progress_);
    progress_ = p;
    return progress_;
  }
}
/* namespace energi */
