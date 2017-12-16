/*
 * Miner.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_MINER_H_
#define ENERGIMINER_MINER_H_

#include "energiminer/plant.h"
#include "energiminer/worker.h"

#include <string>
#include <atomic>
#include <chrono>

namespace energi
{
  class Miner : public Worker
  {
  public:
    Miner(std::string const& name, const Plant &plant, int index):
      Worker(name + std::to_string(index))
      ,index_(index)
      ,plant_(plant)
      ,nonceStart_(0)
      ,nonceEnd_(0)
      ,hashCount_(0)
    {}

    Miner(Miner && m) = default;
    virtual ~Miner() = default;

    void setWork(const Work& work, uint32_t nonceStart, uint32_t nonceEnd)
    {
      Worker::setWork(work);
      nonceStart_ = nonceStart;
      nonceEnd_ = nonceEnd;
      resetHashCount();
    }

    uint32_t hashCount() const { return hashCount_.load(); }
    void resetHashCount() { hashCount_ = 0; }

    protected:
      void addHashCount(uint32_t _n) { hashCount_ += _n; }
      int index_ = 0;
      const Plant &plant_;


      std::atomic<uint32_t> nonceStart_;
      std::atomic<uint32_t> nonceEnd_;
    private:
      std::atomic<uint32_t> hashCount_;
      std::chrono::high_resolution_clock::time_point workSwitchStart_;
  };


  using MinerPtr = std::shared_ptr<energi::Miner>;
  using Miners = std::vector<MinerPtr>;

} /* namespace energi */

#endif /* ENERGIMINER_MINER_H_ */
