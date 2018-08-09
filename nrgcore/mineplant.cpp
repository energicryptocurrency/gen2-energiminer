/*
 * MinePlant.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include "mineplant.h"

#ifdef ETH_ETHASHCL
#include "libegihash-cl/OpenCLMiner.h"
#endif

#ifdef ETH_ETHASHCUDA
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
#if ETH_ETHASHCL
    if (minerEngine == EnumMinerEngine::kCL) {
        return MinerPtr(new OpenCLMiner(plant, index));
    }
#endif
#if ETH_ETHASHCUDA
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

MinePlant::MinePlant(boost::asio::io_service & io_service)
    : m_io_strand(io_service)
    , m_hashrateTimer(io_service)
{
    std::random_device engine;
    m_nonceScumbler = std::uniform_int_distribution<uint64_t>()(engine);

    // Init HWMON
    adlh = wrap_adl_create();
#if defined(__linux)
    sysfsh = wrap_amdsysfs_create();
#endif
    nvmlh = wrap_nvml_create();
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
}

bool MinePlant::start(const std::vector<EnumMinerEngine> &vMinerEngine)
{
    std::lock_guard<std::mutex> lock(x_minerWork);
    if (isMining()) {
        return true;
    }
    //m_started = true;
    for ( auto &minerEngine : vMinerEngine) {
        unsigned count = 0;
#if ETH_ETHASHCL
        if (minerEngine == EnumMinerEngine::kCL) {
            count = OpenCLMiner::instances();
        }
#endif
#if ETH_ETHASHCUDA
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
            m_miners.back()->startWorking();
        }
    }
    m_isMining.store(true, std::memory_order_relaxed);

    // Start hashrate collector
    m_hashrateTimer.cancel();
    m_hashrateTimer.expires_from_now(boost::posix_time::milliseconds(1000));
    m_hashrateTimer.async_wait(m_io_strand.wrap(boost::bind(&MinePlant::processHashRate, this, boost::asio::placeholders::error)));

    return true;
}

void MinePlant::stop()
{
    if (isMining()) {
        {
            std::lock_guard<std::mutex> lock(x_minerWork);
            m_miners.clear();
            m_isMining = false;
        }

        m_hashrateTimer.cancel();

        m_lastProgresses.clear();
    }
}

void MinePlant::setWork(const Work& work)
{
    //Collect hashrate before miner reset their work
    collectHashRate();
    std::lock_guard<std::mutex> lock(x_minerWork);
    // if new work hasnt changed, then ignore
    if (work == m_work) {
        for (auto& miner : m_miners) {
            miner->startWorking();
        }
    }
    cnote << "New Work assigned: Height: "
          << work.nHeight
          << "PrevHash:"
          << work.hashPrevBlock.ToString();
    m_work = work;

    // Propagate to all miners
    for (auto &miner: m_miners) {
        miner->setWork(work);
    }
}

void MinePlant::submitProof(const Solution& solution) const
{
    assert(m_onSolutionFound);
    m_onSolutionFound(solution);
}

void MinePlant::collectHashRate()
{
    std::lock_guard<std::mutex> lock(x_minerWork);

    auto now = std::chrono::steady_clock::now();

    WorkingProgress p;
    p.ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastStart).count();
    m_lastStart = now;

    //Collect
    for (auto const& miner : m_miners) {
        auto minerHashCount = miner->RetrieveAndClearHashCount();
        p.hashes += minerHashCount;
        p.minersHashes.insert(std::make_pair<std::string, uint64_t>(miner->name(), std::move(minerHashCount)));
    }
    if (p.hashes > 0) {
        m_lastProgresses.push_back(p);
    }
    // We smooth the hashrate over the last x seconds
    uint64_t allMs = 0;
    for (auto const& cp : m_lastProgresses){
        allMs += cp.ms;
    }
    if (allMs > m_hashrateSmoothInterval) {
        m_lastProgresses.erase(m_lastProgresses.begin());
    }
}

/*
 * @brief Get information on the progress of mining this work.
 * @return The progress with mining so far.
 */
const WorkingProgress& MinePlant::miningProgress(bool hwmon, bool power) const
{
    std::lock_guard<std::mutex> lock(x_minerWork);
    WorkingProgress p;
    p.ms = 0;
    p.hashes = 0;
    for (auto const& i : m_miners) {
        p.miningIsPaused.insert(std::make_pair<std::string, bool>(i->name(), i->is_mining_paused()));
        p.minersHashes.insert(std::make_pair<std::string, uint64_t>(i->name(), 0));
        if (hwmon) {
            HwMonitorInfo hwInfo = i->hwmonInfo();
            HwMonitor hw;
            unsigned int tempC = 0, fanpcnt = 0, powerW = 0;
            if (hwInfo.deviceIndex >= 0) {
                if (hwInfo.deviceType == HwMonitorInfoType::NVIDIA && nvmlh) {
                    int typeidx = 0;
                    if (hwInfo.indexSource == HwMonitorIndexSource::CUDA){
                        typeidx = nvmlh->cuda_nvml_device_id[hwInfo.deviceIndex];
                    } else if (hwInfo.indexSource == HwMonitorIndexSource::OPENCL){
                        typeidx = nvmlh->opencl_nvml_device_id[hwInfo.deviceIndex];
                    } else {
                        //Unknown, don't map
                        typeidx = hwInfo.deviceIndex;
                    }
                    wrap_nvml_get_tempC(nvmlh, typeidx, &tempC);
                    wrap_nvml_get_fanpcnt(nvmlh, typeidx, &fanpcnt);
                    if (power) {
                        wrap_nvml_get_power_usage(nvmlh, typeidx, &powerW);
                    }
                } else if (hwInfo.deviceType == HwMonitorInfoType::AMD && adlh) {
                    int typeidx = 0;
                    if (hwInfo.indexSource == HwMonitorIndexSource::OPENCL) {
                        typeidx = adlh->opencl_adl_device_id[hwInfo.deviceIndex];
                    } else {
                        //Unknown, don't map
                        typeidx = hwInfo.deviceIndex;
                    }
                    wrap_adl_get_tempC(adlh, typeidx, &tempC);
                    wrap_adl_get_fanpcnt(adlh, typeidx, &fanpcnt);
                    if (power) {
                        wrap_adl_get_power_usage(adlh, typeidx, &powerW);
                    }
                }
#if defined(__linux)
                // Overwrite with sysfs data if present
                if (hwInfo.deviceType == HwMonitorInfoType::AMD && sysfsh) {
                    int typeidx = 0;
                    if (hwInfo.indexSource == HwMonitorIndexSource::OPENCL){
                        typeidx = sysfsh->opencl_sysfs_device_id[hwInfo.deviceIndex];
                    } else {
                        //Unknown, don't map
                        typeidx = hwInfo.deviceIndex;
                    }
                    wrap_amdsysfs_get_tempC(sysfsh, typeidx, &tempC);
                    wrap_amdsysfs_get_fanpcnt(sysfsh, typeidx, &fanpcnt);
                    if (power) {
                        wrap_amdsysfs_get_power_usage(sysfsh, typeidx, &powerW);
                    }
                }
#endif
            }
            i->update_temperature(tempC);

            hw.tempC = tempC;
            hw.fanP = fanpcnt;
            hw.powerW = powerW/((double)1000.0);
            p.minerMonitors[i->name()] = hw;
        }
    }

    for (auto const& cp : m_lastProgresses) {
        p.ms += cp.ms;
        p.hashes += cp.hashes;
        for (auto const & i : cp.minersHashes) {
            p.minersHashes.at(i.first) += i.second;
        }
    }
    m_progress = p;
    return m_progress;
}

void MinePlant::setTStartTStop(unsigned tstart, unsigned tstop)
{
    m_tstart = tstart;
    m_tstop = tstop;
}

void MinePlant::processHashRate(const boost::system::error_code& ec)
{
    if (!ec) {
        if (!isMining()) {
            return;
        }
        collectHashRate();
        // Restart timer
        m_hashrateTimer.expires_from_now(boost::posix_time::milliseconds(1000));
        m_hashrateTimer.async_wait(m_io_strand.wrap(boost::bind(&MinePlant::processHashRate, this, boost::asio::placeholders::error)));
    }
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

uint64_t MinePlant::get_start_nonce(const Work& work, unsigned idx) const
{
    static uint64_t range = std::numeric_limits<uint64_t>::max() / m_miners.size();
    return work.startNonce + range * idx;
}

uint64_t MinePlant::get_nonce_scumbler() const
{
    return m_nonceScumbler;
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

