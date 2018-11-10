/*
 * MinePlant.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include "mineplant.h"

#ifdef NRGHASHCL
#include "libnrghash-cl/CLMiner.h"
#endif

#ifdef NRGHASHCUDA
#include "libnrghash-cuda/CUDAMiner.h"
#endif

#include "nrgcore/miner.h"
#include "primitives/work.h"
#include "energiminer/CpuMiner.h"
#include "energiminer/TestMiner.h"

#include "common/common.h"
#include "common/Log.h"

#include <boost/bind.hpp>
#include <iostream>
#include <limits>

using namespace energi;


MinerPtr createMiner(EnumMinerEngine minerEngine, int index, const MinePlant &plant)
{
#if NRGHASHCL
    if (minerEngine == EnumMinerEngine::kCL) {
        return MinerPtr(new CLMiner(plant, index));
    }
#endif
#if NRGHASHCUDA
    if (minerEngine == EnumMinerEngine::kCUDA) {
        return MinerPtr(new CUDAMiner(plant, index));
    }
#endif
    if (minerEngine == EnumMinerEngine::kCPU) {
        return MinerPtr(new CpuMiner(plant, index));
    }
    if (minerEngine == EnumMinerEngine::kTest) {
        return MinerPtr(new TestMiner(plant, index));
    }
    return nullptr;
}

MinePlant::MinePlant(boost::asio::io_service& io_service, bool hwmon, bool pwron)
    : m_io_strand(io_service)
    , m_collectTimer(io_service)
{
    m_hwmon = hwmon;
    m_pwron = pwron;

    // Init HWMON if needed
    if (m_hwmon) {
        adlh = wrap_adl_create();
#if defined(__linux)
        sysfsh = wrap_amdsysfs_create();
#endif
        nvmlh = wrap_nvml_create();
    }

    // Start data collector timer
    // It should work for the whole lifetime of Farm
    // regardless it's mining state
    m_lastHashRate = std::chrono::steady_clock::now();
    
    m_collectTimer.expires_from_now(boost::posix_time::milliseconds(m_collectInterval));
    m_collectTimer.async_wait(m_io_strand.wrap(
                boost::bind(&MinePlant::collectData, this, boost::asio::placeholders::error)));
}

MinePlant::~MinePlant()
{
    // Deinit HWMON
    if (adlh) {
        wrap_adl_destroy(adlh);
    }
#if defined(__linux)
    if (sysfsh) {
        wrap_amdsysfs_destroy(sysfsh);
    }
#endif
    if (nvmlh) {
        wrap_nvml_destroy(nvmlh);
    }
    stop();
    // Stop data collector
    m_collectTimer.cancel();
}

bool MinePlant::start(const std::vector<EnumMinerEngine> &vMinerEngine)
{
    std::lock_guard<std::mutex> lock(x_minerWork);
    if (isMining()) {
        return true;
    }
    
    m_lastHashRate = std::chrono::steady_clock::now();

    //m_started = true;
    for ( auto &minerEngine : vMinerEngine) {
        unsigned count = 0;
#if NRGHASHCL
        if (minerEngine == EnumMinerEngine::kCL) {
            count = CLMiner::instances();
        }
#endif
#if NRGHASHCUDA
        if (minerEngine == EnumMinerEngine::kCUDA) {
            count = CUDAMiner::instances();
        }
#endif
        if (minerEngine == EnumMinerEngine::kTest) {
            count = 2;
        }
        if (minerEngine == EnumMinerEngine::kCPU) {
            count = std::thread::hardware_concurrency() - 1;
        }
        for ( unsigned i = 0; i < count; ++i ) {
            m_miners.push_back(createMiner(minerEngine, i, *this));
            
            auto &miner = m_miners.back();
            
            miner->RetrieveHashRateDiff();
            miner->setWork(m_work);
            miner->startWorking();
        }
    }
    m_isMining.store(true, std::memory_order_relaxed);

    return true;
}

void MinePlant::stop()
{
    if (isMining()) {
        {
            std::lock_guard<std::mutex> lock(x_minerWork);
            m_miners.clear();
            m_isMining.store(false, std::memory_order_relaxed);
        }
    }
}

void MinePlant::setWork(const Work& work)
{
    std::lock_guard<std::mutex> lock(x_minerWork);
    // if new work hasnt changed, then ignore
    if (work == m_work) {
        for (auto& miner : m_miners) {
            miner->startWorking();
        }
    } else {
        m_lastHashRate = std::chrono::steady_clock::now();
    }
    
    cnote << "New Work assigned Height: "
          << work.nHeight
          << " PrevHash: "
          << work.hashPrevBlock.ToString();
    m_work = work;

    // Propagate to all miners
    for (auto &miner: m_miners) {
        miner->RetrieveHashRateDiff();
        miner->setWork(work);
    }
}

void MinePlant::resetWork()
{
    m_work.reset();
    for (auto& miner : m_miners) {
        miner->resetWork();
    }
}

void MinePlant::submitProof(const Solution& solution) const
{
    assert(m_onSolutionFound);
    m_onSolutionFound(solution);
}

void MinePlant::collectData(const boost::system::error_code& ec)
{
    if (ec)
        return;

    WorkingProgress progress;
    
    using namespace std::chrono;
    const auto time_now = steady_clock::now();
    const auto time_diff = time_now - m_lastHashRate;
    m_lastHashRate = time_now;
    const auto time_diff_us = duration_cast<microseconds>(time_diff).count();
    
    const auto alpha = 0.05;

    // Process miner hashrate
    for (auto const& miner : m_miners) {
        // Collect and reset hashrates
        if (!miner->is_mining_paused()) {
            auto hrd = miner->RetrieveHashRateDiff();
            float hr = float(hrd) * 1e6 / time_diff_us;
            auto prev_hr = m_progress.minersHashRates[miner->name()];
            
            if (prev_hr > 1 && hr > 1) {
                // Smooth: https://en.wikipedia.org/wiki/Moving_average#Exponential_moving_average
                hr = (alpha * prev_hr) + ((1.0 - alpha) * hr);
            }
            
            progress.hashRate += hr;
            progress.minersHashRates.insert(std::make_pair<std::string, float>(miner->name(), std::move(hr)));
            progress.miningIsPaused.insert(std::make_pair<std::string, bool>(miner->name(), false));
        } else {
            progress.minersHashRates.insert(std::make_pair<std::string, float>(miner->name(), 0.0));
            progress.miningIsPaused.insert(std::make_pair<std::string, bool>(miner->name(), true));
        }
    }
    
    // Process miner hashrate
    if (m_hwmon) {
        for (auto const& miner : m_miners) {
            HwMonitorInfo hwInfo = miner->hwmonInfo();
            HwMonitor hw;
            unsigned int tempC = 0, fanpcnt = 0, powerW = 0;
            if (hwInfo.deviceIndex >= 0) {
                if (hwInfo.deviceType == HwMonitorInfoType::NVIDIA && nvmlh) {
                    int typeidx = 0;
                    if (hwInfo.indexSource == HwMonitorIndexSource::CUDA) {
                        typeidx = nvmlh->cuda_nvml_device_id[hwInfo.deviceIndex];
                    } else if (hwInfo.indexSource == HwMonitorIndexSource::OPENCL) {
                        typeidx = nvmlh->opencl_nvml_device_id[hwInfo.deviceIndex];
                    } else {
                        // Unknown, don't map
                        typeidx = hwInfo.deviceIndex;
                    }
                    wrap_nvml_get_tempC(nvmlh, typeidx, &tempC);
                    wrap_nvml_get_fanpcnt(nvmlh, typeidx, &fanpcnt);
                    if (m_pwron) {
                        wrap_nvml_get_power_usage(nvmlh, typeidx, &powerW);
                    }
                } else if (hwInfo.deviceType == HwMonitorInfoType::AMD && adlh) {
                    int typeidx = 0;
                    if (hwInfo.indexSource == HwMonitorIndexSource::OPENCL) {
                        typeidx = adlh->opencl_adl_device_id[hwInfo.deviceIndex];
                    } else {
                        // Unknown, don't map
                        typeidx = hwInfo.deviceIndex;
                    }
                    wrap_adl_get_tempC(adlh, typeidx, &tempC);
                    wrap_adl_get_fanpcnt(adlh, typeidx, &fanpcnt);
                    if (m_pwron) {
                        wrap_adl_get_power_usage(adlh, typeidx, &powerW);
                    }
                }
#if defined(__linux)
                // Overwrite with sysfs data if present
                if (hwInfo.deviceType == HwMonitorInfoType::AMD && sysfsh) {
                    int typeidx = 0;
                    if (hwInfo.indexSource == HwMonitorIndexSource::OPENCL) {
                        typeidx = sysfsh->opencl_sysfs_device_id[hwInfo.deviceIndex];
                    } else {
                        // Unknown, don't map
                        typeidx = hwInfo.deviceIndex;
                    }
                    wrap_amdsysfs_get_tempC(sysfsh, typeidx, &tempC);
                    wrap_amdsysfs_get_fanpcnt(sysfsh, typeidx, &fanpcnt);
                    if (m_pwron) {
                        wrap_amdsysfs_get_power_usage(sysfsh, typeidx, &powerW);
                    }
                }
#endif
            }

            miner->update_temperature(tempC);

            hw.tempC = tempC;
            hw.fanP = fanpcnt;
            hw.powerW = powerW / ((double)1000.0);
            progress.minerMonitors[miner->name()] = hw;
        }
    }

    m_progress = progress;

    // Resubmit timer for another loop
    m_collectTimer.expires_from_now(boost::posix_time::milliseconds(m_collectInterval));
    m_collectTimer.async_wait(m_io_strand.wrap(
                boost::bind(&MinePlant::collectData, this, boost::asio::placeholders::error)));
}


void MinePlant::setTStartTStop(unsigned tstart, unsigned tstop)
{
    m_tstart = tstart;
    m_tstop = tstop;
}

void MinePlant::restart()
{
    if (m_onMinerRestart) {
        m_onMinerRestart();
    }
}

bool MinePlant::isMining() const
{
    return m_isMining;
}

uint64_t MinePlant::getStartNonce(const Work& work, unsigned idx) const
{
    static uint64_t range = std::numeric_limits<uint64_t>::max() / m_miners.size();
    return work.startNonce + range * idx;
}

SolutionStats MinePlant::getSolutionStats()
{
    return m_solutionStats;
}

void MinePlant::failedSolution()
{
    m_solutionStats.failed();
}

void MinePlant::acceptedSolution(bool _stale)
{
    if (!_stale) {
        m_solutionStats.accepted();
    } else {
        m_solutionStats.acceptedStale();
    }
}

void MinePlant::rejectedSolution()
{
    m_solutionStats.rejected();
}

const Work& MinePlant::getWork() const
{
    std::lock_guard<std::mutex> lock(x_minerWork);
    return m_work;
}

std::chrono::steady_clock::time_point MinePlant::farmLaunched()
{
    return m_farm_launched;
}

std::string MinePlant::farmLaunchedFormatted() const
{
    auto d = std::chrono::steady_clock::now() - m_farm_launched;
    int hsize = 3;
    auto hhh = std::chrono::duration_cast<std::chrono::hours>(d);
    if (hhh.count() < 100) {
        hsize = 2;
    }
    d -= hhh;
    auto mm = std::chrono::duration_cast<std::chrono::minutes>(d);
    std::ostringstream stream;
    stream << "Time: " << std::setfill('0') << std::setw(hsize) << hhh.count() << ':' << std::setfill('0') << std::setw(2) << mm.count();
    return stream.str();
	}

void MinePlant::set_pool_addresses(const std::string& host, unsigned port)
{
    std::stringstream ssPoolAddresses;
    ssPoolAddresses << host << ':' << port;
    m_pool_addresses = ssPoolAddresses.str();
}

const std::string& MinePlant::get_pool_addresses() const
{
    return m_pool_addresses;
}


