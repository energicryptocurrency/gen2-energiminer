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
#include "energiminer/egihash/egihash.h"

#include <string>
#include <atomic>
#include <chrono>

#include <tuple>
#include <memory>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace energi {

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
    {
        // First time init egi hash dag
        // We got to do for every epoch change below
        InitEgiHashDag();
    }

    Miner(Miner && m) = default;
    virtual ~Miner() = default;

public:
    void setWork(const Work& work, uint32_t nonceStart, uint32_t nonceEnd)
    {
        Worker::setWork(work);
        nonceStart_ = nonceStart;
        nonceEnd_ = nonceEnd;
        resetHashCount();
    }

    uint64_t get_start_nonce() const
    {
        // Each GPU is given a non-overlapping 2^40 range to search
        return plant_.get_nonce_scumbler() + ((uint64_t) index_ << 40);
    }

    void stopMining()
    {
        stopAllWork();
    }

    uint32_t hashCount() const
    {
        return hashCount_.load();
    }
    void resetHashCount()
    {
        hashCount_ = 0;
    }

    //! static interfaces
public:
    static bool InitEgiHashDag();
    static boost::filesystem::path GetDataDir();
    static egihash::h256_t GetHeaderHash(const void *input);
    static void InitDAG(egihash::progress_callback_type callback);
    static egihash::result_t GetPOWHash(uint32_t height, uint32_t nonce, const void *input);

    static std::unique_ptr<egihash::dag_t> const & ActiveDAG(std::unique_ptr<egihash::dag_t> next_dag  = std::unique_ptr<egihash::dag_t>());

protected:
    void addHashCount(uint32_t _n)
    {
        hashCount_ += _n;
    }

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
