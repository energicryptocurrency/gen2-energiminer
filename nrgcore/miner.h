/*
 * Miner.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_MINER_H_
#define ENERGIMINER_MINER_H_

#include "nrgcore/plant.h"
#include "primitives/worker.h"
#include "nrghash/nrghash.h"

#include <string>
#include <atomic>
#include <chrono>

#include <tuple>
#include <memory>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#define LOG2_MAX_MINERS 5u
#define MAX_MINERS (1u << LOG2_MAX_MINERS)

#define DAG_LOAD_MODE_PARALLEL	 0
#define DAG_LOAD_MODE_SEQUENTIAL 1
#define DAG_LOAD_MODE_SINGLE	 2

namespace energi {

class Miner : public Worker
{
public:
    Miner(const std::string& name, const Plant &plant, unsigned index)
        : Worker(name + std::to_string(index))
        , m_lastHeight(0)
        , m_index(index)
        , m_plant(plant)
        , m_hashCount(0)
    {
    }

    virtual ~Miner() = default;

public:
    void setWork(const Work& work)
    {
        {
            std::lock_guard<std::mutex> lock(x_work);
            m_work = work;
            m_work.incrementExtraNonce();
            m_newWorkAssigned = true;
        }
        onSetWork();
    }

    uint32_t hashCount() const
    {
        return m_hashCount.load(std::memory_order_relaxed);
    }
    void resetHashCount()
    {
        m_hashCount.store(0, std::memory_order_relaxed);
    }

	unsigned Index() { return m_index; };
	HwMonitorInfo& hwmonInfo() { return m_hwmoninfo; }

    uint64_t get_start_nonce() const
    {
        // Each GPU is given a non-overlapping 2^40 range to search
        return m_plant.get_nonce_scumbler() + ((uint64_t) m_index << 40);
    }

    void updateWorkTimestamp()
    {
        static std::recursive_mutex s_rMutex;
        std::lock_guard<std::recursive_mutex> lock(s_rMutex);
        m_work.updateWorkTimeistamp()
    }

	void update_temperature(unsigned temperature);
	bool is_mining_paused() const;

    void set_mining_paused(MinigPauseReason pause_reason);
    void clear_mining_paused(MinigPauseReason pause_reason);

    //! static interfaces
public:
    static bool LoadNrgHashDAG(uint64_t blockHeight = 0);
    static boost::filesystem::path GetDataDir();
    static void InitDAG(uint64_t blockHeight, nrghash::progress_callback_type callback);
    static uint256 GetPOWHash(const BlockHeader& header);

    static std::unique_ptr<nrghash::dag_t> const & ActiveDAG(std::unique_ptr<nrghash::dag_t> next_dag  = std::unique_ptr<nrghash::dag_t>());

protected:
	/**
	 * @brief No work left to be done. Pause until told to kickOff().
	 */
    virtual void onSetWork() {}

    Work getWork() const
    {
        std::lock_guard<std::mutex> lock(x_work);
        return m_work;
    }

    void addHashCount(uint32_t _n)
    {
        m_hashCount.fetch_add(_n, std::memory_order_relaxed);
    }
    static unsigned s_dagLoadMode;
    static unsigned s_dagLoadIndex;
    static unsigned s_dagCreateDevice;
    static uint8_t* s_dagInHostMemory;
    static bool s_exit;
    static bool s_noeval;

    bool     m_newWorkAssigned = false;
    bool     m_dagLoaded = false;
    uint64_t m_lastHeight;

    unsigned m_index = 0;
    const Plant &m_plant;
    std::chrono::high_resolution_clock::time_point workSwitchStart_;
	HwMonitorInfo m_hwmoninfo;

private:
    MiningPause m_mining_paused;
    std::atomic<uint32_t> m_hashCount;
	Work m_work;
	mutable std::mutex x_work;
};

using MinerPtr = std::shared_ptr<energi::Miner>;
using Miners = std::vector<MinerPtr>;

} /* namespace energi */

#endif /* ENERGIMINER_MINER_H_ */
