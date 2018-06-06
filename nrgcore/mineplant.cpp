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

#include <iostream>
#include <limits>

namespace energi {

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

bool MinePlant::start(const std::vector<EnumMinerEngine> &vMinerEngine)
{
    if (isStarted()) {
        return true;
    }
    m_started = true;
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
            auto miner = createMiner(minerEngine, i, *this);
            m_started &= miner->start();
            if ( m_started ) {
                m_miners.push_back(miner);
            }
        }
    }

    if (m_tHashrateTimer) {
        std::cerr << "Hash Rate Thread active !, weird" << std::endl;
        return false;
    } else {
        m_tHashrateTimer.reset(new std::thread([&]() {
            while(m_continueHashTimer) {
                collectHashRate();
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }));
    }

    // Failure ? return right away
    return m_started;
}

void MinePlant::stop()
{
    for (auto &miner : m_miners) {
        miner->stop();
    }

    if(m_tHashrateTimer) {
        m_continueHashTimer = false;
        m_tHashrateTimer->join();
        m_tHashrateTimer.reset();
    }
    m_started = false;
}

bool MinePlant::setWork(const Work& work)
{
    //Collect hashrate before miner reset their work
    collectHashRate();
    // if new work hasnt changed, then ignore
    std::lock_guard<std::mutex> lock(m_mutexWork);
    if (work == m_work) {
        for (auto& miner : m_miners) {
            reinterpret_cast<Worker*>(miner.get())->setWork(m_work);
        }
        return false;
    }
    cnote << "New Work assigned: Height: "
          << work.nHeight
          << "PrevHash:"
          << work.hashPrevBlock.ToString();
    m_work = work;

    // Propagate to all miners
    const auto minersCount = m_miners.size();
    const auto kLimitPerThread = std::numeric_limits<uint32_t>::max() / minersCount;
    uint32_t index = 0;
    m_solutionFound = false;
    for (auto &miner: m_miners) {
        auto first  = index * kLimitPerThread;
        auto end    = first + kLimitPerThread;
        //cdebug << "first:" << first << "end: " << end;
        miner->setWork(work, first, end);
        ++index;
    }
    return true;
}

void MinePlant::stopAllWork()
{
    std::lock_guard<std::mutex> lock(m_mutexWork);
    for (auto &miner: m_miners) {
        miner->stopMining();
    }
}

void MinePlant::submit(const Solution &solution) const
{
    solution_found_cb_(solution);
}

void MinePlant::collectHashRate()
{
    WorkingProgress p;
    std::lock_guard<std::mutex> lock(m_mutexWork);
    p.ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_lastStart).count();

    //Collect
    for (auto const& miner : m_miners) {
        auto minerHashCount = miner->hashCount();
        p.hashes += minerHashCount;
        p.minersHashes.insert(std::make_pair<std::string, uint64_t>(miner->name(), uint64_t(minerHashCount)));
    }

    //Reset miner hashes
    for (auto const& miner : m_miners) {
        miner->resetHashCount();
    }
    m_lastStart = std::chrono::steady_clock::now();
    if (p.hashes > 0) {
        m_lastProgresses.push_back(p);
    }
    // We smooth the hashrate over the last x seconds
    int allMs = 0;
    for (auto const& cp : m_lastProgresses) {
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
WorkingProgress const& MinePlant::miningProgress() const
{
    WorkingProgress p;
    p.ms = 0;
    p.hashes = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutexWork);
        for (auto const& miner : m_miners) {
            p.minersHashes.insert(std::make_pair<std::string, uint64_t>(miner->name(), 0));
        }
        for (auto const& cp : m_lastProgresses) {
            p.ms += cp.ms;
            p.hashes += cp.hashes;
            for (auto const & i : cp.minersHashes) {
                p.minersHashes.at(i.first) += i.second;
            }
        }
    }

    std::lock_guard<std::mutex> lock(m_mutexProgress);
    m_progress = p;
    return m_progress;
}

} //! namespace energi
