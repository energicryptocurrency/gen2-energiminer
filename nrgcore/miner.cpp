/*
 * Miner.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include <iomanip>
#include <mutex>
#include <iostream>
#include <sstream>

#include "miner.h"

using namespace energi;

bool Miner::s_exit = false;

unsigned Miner::s_dagLoadMode = 0;

unsigned Miner::s_dagLoadIndex = 0;

unsigned Miner::s_dagCreateDevice = 0;

uint8_t* Miner::s_dagInHostMemory = nullptr;

bool Miner::s_noeval = false;

void Miner::updateHashRate(uint64_t _n)
{
    using namespace std::chrono;
    steady_clock::time_point t = steady_clock::now();
    auto us = duration_cast<microseconds>(t - m_hashTime).count();
    m_hashTime = t;
    float hr = 0.0;
    if (us)
        hr = (float(_n) * 1.0e6f) / us;
    m_hashRate.store(hr, std::memory_order_relaxed);
}

bool Miner::LoadNrgHashDAG(uint64_t blockHeight)
{
    // initialize the DAG
    InitDAG(blockHeight, [](::std::size_t step, ::std::size_t max, int phase) -> bool {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2)
           << static_cast<double>(step) / static_cast<double>(max) * 100.0 << "%"
           << std::setfill(' ') << std::setw(80);

        auto progress_handler = [&](std::string const &msg) {
            std::cout << "\r" << msg;
        };

        switch(phase) {
        case nrghash::cache_seeding:
            progress_handler("Seeding cache ... ");
            break;
        case nrghash::cache_generation:
            progress_handler("Generating cache ... ");
            break;
        case nrghash::cache_saving:
            progress_handler("Saving cache ... ");
            break;
        case nrghash::cache_loading:
            progress_handler("Loading cache ... ");
            break;
        case nrghash::dag_generation:
            progress_handler("Generating Dag ... ");
            break;
        case nrghash::dag_saving:
            progress_handler("Saving Dag ... ");
            break;
        case nrghash::dag_loading:
            progress_handler("Loading Dag ... ");
            break;
        default:
            break;
        }
        auto progress = ss.str();
        std::cout << progress << std::flush;
        return true;
    });
    return true;
}

uint256 Miner::GetPOWHash(const BlockHeader& header)
{
    energi::CBlockHeaderTruncatedLE truncatedBlockHeader(header);
    nrghash::h256_t headerHash(&truncatedBlockHeader, sizeof(truncatedBlockHeader));

    nrghash::result_t ret;
    const auto& dag = ActiveDAG();
    if (dag && (header.nHeight / nrghash::constants::EPOCH_LENGTH) == dag->epoch()) {
        ret = nrghash::full::hash(*dag, headerHash, header.nNonce);
    } else {
        ret = nrghash::light::hash(nrghash::cache_t(header.nHeight), headerHash, header.nNonce);
    }
    const_cast<BlockHeader&>(header).hashMix = uint256(ret.mixhash);
    return uint256(ret.value);
}

std::unique_ptr<nrghash::dag_t> const & Miner::ActiveDAG(std::unique_ptr<nrghash::dag_t> next_dag)
{
    using namespace std;

    static std::mutex m;
    std::lock_guard<std::mutex> lock(m);
    static std::unique_ptr<nrghash::dag_t> active; // only keep one DAG in memory at once

    // if we have a next_dag swap it
    if (next_dag) {
        active.swap(next_dag);
    }
    // unload the previous dag
    if (next_dag) {
        next_dag->unload();
        next_dag.reset();
    }
    return active;
}

boost::filesystem::path Miner::GetDataDir()
{
    namespace fs = boost::filesystem;
#ifdef WIN32
    return fs::path(getenv("APPDATA") + std::string("/EnergiCore/energiminer"));
#else
    fs::path result;
    char* homePath = getenv("HOME");
    if (homePath == nullptr || strlen(homePath) == 0) {
        result = fs::path("/");
    } else {
        result = fs::path(homePath);
    }
#ifdef MAC_OSX
    return result / "Library/Application Support/EnergiCore/energiminer";
#else
    return result /".energicore/energiminer";
#endif
#endif
}

void Miner::InitDAG(uint64_t blockHeight, nrghash::progress_callback_type callback)
{
    using namespace nrghash;

    auto const & dag = ActiveDAG();
    if (!dag) {
        auto const epoch = blockHeight / constants::EPOCH_LENGTH;
        auto const & seedhash = cache_t::get_seedhash(0).to_hex();
        std::stringstream ss;
        ss << std::hex << std::setw(4) << std::setfill('0') << epoch << "-" << seedhash.substr(0, 12) << ".dag";
        auto const epoch_file = GetDataDir() / "dag" / ss.str();

        std::cout << "\nDAG file for epoch " << epoch << " is " << epoch_file.string() << std::endl;
        // try to load the DAG from disk
        try {
            std::unique_ptr<dag_t> new_dag(new dag_t(epoch_file.string(), callback));
            ActiveDAG(move(new_dag));
            std::cout << "\nDAG file " << epoch_file.string() << " loaded successfully. \n\n\n";

            return;
        } catch (hash_exception const & e) {
            std::cout << "\nDAG file " << epoch_file.string() << " not loaded, will be generated instead. Message: \n" << e.what() << std::endl;
        }
        // try to generate the DAG
        try {
            std::unique_ptr<dag_t> new_dag(new dag_t(blockHeight, callback));
            boost::filesystem::create_directories(epoch_file.parent_path());
            new_dag->save(epoch_file.string());
            ActiveDAG(move(new_dag));
            std::cout << "\nDAG generated successfully. Saved to " << epoch_file.string() << std::endl;
        } catch (hash_exception const & e) {
            std::cout << "\nDAG for epoch " << epoch << " could not be generated: " << e.what() << std::endl;
        }
    }
    std::cout << "\nDAG has been initialized already. Use ActiveDAG() to swap.\n" << std::endl;
}

void Miner::update_temperature(unsigned temperature)
{
    /*
       cnote << "Setting temp" << temperature << " for gpu" << index <<
       " tstop=" << farm.get_tstop() << " tstart=" << farm.get_tstart();
       */
    bool _wait_for_tstart_temp = (m_mining_paused.get_mining_paused() &
                                 MinigPauseReason::MINING_PAUSED_WAIT_FOR_T_START) == MinigPauseReason::MINING_PAUSED_WAIT_FOR_T_START;
    if(!_wait_for_tstart_temp) {
        unsigned tstop = m_plant.get_tstop();
        if (tstop && temperature >= tstop) {
            cwarn << "Pause mining on gpu" << m_index << " due -tstop";
            m_mining_paused.set_mining_paused(MinigPauseReason::MINING_PAUSED_WAIT_FOR_T_START);
        }
    } else {
        unsigned tstart = m_plant.get_tstart();
        if (tstart && temperature <= tstart) {
            cnote << "(Re)starting mining on gpu" << m_index << " due -tstart";
            m_mining_paused.clear_mining_paused(MinigPauseReason::MINING_PAUSED_WAIT_FOR_T_START);
        }
    }
}

bool Miner::is_mining_paused() const
{
    return m_mining_paused.is_mining_paused();
}

void Miner::set_mining_paused(MinigPauseReason pause_reason)
{
    m_mining_paused.set_mining_paused(pause_reason);
}

void Miner::clear_mining_paused(MinigPauseReason pause_reason)
{
    m_mining_paused.clear_mining_paused(pause_reason);
}

void Miner::setWork(const Work& work)
{
    {
        std::lock_guard<std::mutex> lock(work_mutex);
        
        if (work == m_work) {
            return;
        }
        
        m_work = work;
        m_work.updateExtraNonce();
        m_newWorkAssigned.store(true, std::memory_order_release);
        if (g_logVerbosity >= 6)
            workSwitchStart = std::chrono::steady_clock::now();
    }

    work_cond.notify_one();
    kick_miner();
}

void Miner::resetWork()
{
    {
        std::lock_guard<std::mutex> lock(work_mutex);
        m_work.reset();
        m_newWorkAssigned.store(true, std::memory_order_release);
    }
    work_cond.notify_one();
}

void Miner::updateWorkTimestamp()
{
    static std::recursive_mutex s_rMutex;
    std::lock_guard<std::recursive_mutex> lock(s_rMutex);
    m_work.updateTimestamp();
}
