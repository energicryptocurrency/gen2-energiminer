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
    Miner(const std::string& name, const Plant &plant, int index)
        : Worker(name + std::to_string(index))
        , m_index(index)
        , m_plant(plant)
        , m_nonceStart(0)
        , m_nonceEnd(0)
        , m_hashCount(0)
    {
    }

    Miner(Miner && m) = default;
    virtual ~Miner() = default;

public:
    void setWork(const Work& work, uint64_t nonceStart, uint64_t nonceEnd)
    {
        Worker::setWork(work);
        m_nonceStart = nonceStart;
        m_nonceEnd = nonceEnd;
        resetHashCount();
    }

    uint64_t get_start_nonce() const
    {
        // Each GPU is given a non-overlapping 2^40 range to search
        return m_plant.get_nonce_scumbler() + ((uint64_t) m_index << 40);
    }

    void stopMining()
    {
        stopAllWork();
    }

    uint32_t hashCount() const
    {
        return m_hashCount.load();
    }
    void resetHashCount()
    {
        m_hashCount = 0;
    }

    //! static interfaces
public:
    static bool LoadNrgHashDAG();
    static boost::filesystem::path GetDataDir();
    static void InitDAG(egihash::progress_callback_type callback);
    static uint256 GetPOWHash(const BlockHeader& header);

    static std::unique_ptr<egihash::dag_t> const & ActiveDAG(std::unique_ptr<egihash::dag_t> next_dag  = std::unique_ptr<egihash::dag_t>());

protected:
    void addHashCount(uint32_t _n)
    {
        m_hashCount += _n;
    }

    int m_index = 0;
    const Plant &m_plant;

    std::atomic<uint64_t> m_nonceStart;
    std::atomic<uint64_t> m_nonceEnd;

private:
    std::atomic<uint32_t> m_hashCount;
    std::chrono::high_resolution_clock::time_point workSwitchStart_;
};

using MinerPtr = std::shared_ptr<energi::Miner>;
using Miners = std::vector<MinerPtr>;

} /* namespace energi */

#endif /* ENERGIMINER_MINER_H_ */
