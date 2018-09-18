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

class FormattedMemSize
{
public:
    explicit FormattedMemSize(uint64_t s) noexcept
    {
        m_size = s;
    }

    uint64_t m_size;
};

inline std::ostream& operator<<(std::ostream& os, const FormattedMemSize& s)
{
    static const char* suffixes[] = {"bytes", "KB", "MB", "GB"};
    double d = double(s.m_size);
    unsigned i;
    for (i = 0; i < 3; i++) {
        if (d < 1024.0)
            break;
        d /= 1024.0;
    }
    return os << std::fixed << std::setprecision(3) << d << ' ' << suffixes[i];
}

class Miner : public Worker
{
public:
    Miner(const std::string& name, const Plant &plant, unsigned index)
        : Worker(name + std::to_string(index))
        , m_lastHeight(0)
        , m_index(index)
        , m_plant(plant)
    {
    }

    virtual ~Miner() = default;

public:
    void setWork(const Work& work);
    void resetWork();


    unsigned Index() { return m_index; };

	HwMonitorInfo& hwmonInfo() { return m_hwmoninfo; }

    void updateWorkTimestamp();

	void update_temperature(unsigned temperature);
	bool is_mining_paused() const;

    float RetrieveHashRate()
    {
        return m_hashRate.load(std::memory_order_relaxed);
    }

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

    const Work& getWork() const
    {
        std::lock_guard<std::mutex> lock(x_work);
        return m_work;
    }

    void updateHashRate(uint64_t _n);

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
    std::chrono::steady_clock::time_point workSwitchStart;
	HwMonitorInfo m_hwmoninfo;

protected:
	Work m_work;
    Work m_current;

private:
    MiningPause m_mining_paused;
	mutable std::mutex x_work;

    std::chrono::steady_clock::time_point m_hashTime = std::chrono::steady_clock::now();
    std::atomic<float> m_hashRate = {0.0};
};

using MinerPtr = std::shared_ptr<energi::Miner>;
using Miners = std::vector<MinerPtr>;

} /* namespace energi */

#endif /* ENERGIMINER_MINER_H_ */
