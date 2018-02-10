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

bool Miner::InitEgiHashDag()
{
    // initialize the DAG
    InitDAG([](::std::size_t step, ::std::size_t max, int phase) -> bool {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2)
           << static_cast<double>(step) / static_cast<double>(max) * 100.0 << "%"
           << std::setfill(' ') << std::setw(80);

        auto progress_handler = [&](std::string const &msg) {
            std::cout << "\r" << msg;
        };

        switch(phase) {
        case egihash::cache_seeding:
            progress_handler("Seeding cache ... ");
            break;
        case egihash::cache_generation:
            progress_handler("Generating cache ... ");
            break;
        case egihash::cache_saving:
            progress_handler("Saving cache ... ");
            break;
        case egihash::cache_loading:
            progress_handler("Loading cache ... ");
            break;
        case egihash::dag_generation:
            progress_handler("Generating Dag ... ");
            break;
        case egihash::dag_saving:
            progress_handler("Saving Dag ... ");
            break;
        case egihash::dag_loading:
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

egihash::result_t Miner::GetPOWHash(uint32_t height, uint32_t nonce, const void *input)
{
    energi::CBlockHeaderTruncatedLE truncatedBlockHeader(input);
    egihash::h256_t headerHash(&truncatedBlockHeader, sizeof(truncatedBlockHeader));
    egihash::result_t ret;
    // if we have a DAG loaded, use it
    auto const & dag = ActiveDAG();

    if (dag && ((height / egihash::constants::EPOCH_LENGTH) == dag->epoch())) {
        ret = egihash::full::hash(*dag, headerHash, nonce);
    } else {
        // otherwise all we can do is generate a light hash
        ret = egihash::light::hash(egihash::cache_t(height), headerHash, nonce);
    }

    auto hashMix = ret.mixhash;
    if (std::memcmp(hashMix.b, egihash::empty_h256.b, egihash::empty_h256.hash_size) == 0) {
        throw WorkException("Can not produce a valid mixhash");
    }
    return ret;
}

egihash::h256_t Miner::GetHeaderHash(const void *input)
{
    energi::CBlockHeaderTruncatedLE truncatedBlockHeader(input);
    return egihash::h256_t(&truncatedBlockHeader, sizeof(truncatedBlockHeader));
}

std::unique_ptr<egihash::dag_t> const & Miner::ActiveDAG(std::unique_ptr<egihash::dag_t> next_dag)
{
    using namespace std;

    static std::mutex m;
    MutexLGuard lock(m);
    static std::unique_ptr<egihash::dag_t> active; // only keep one DAG in memory at once

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

void Miner::InitDAG(egihash::progress_callback_type callback)
{
    using namespace egihash;

    auto const & dag = ActiveDAG();
    if (!dag) {
        auto const height = 0;// TODO (max)(GetHeight(), 0);
        auto const epoch = height / constants::EPOCH_LENGTH;
        auto const & seedhash = cache_t::get_seedhash(0).to_hex();
        std::stringstream ss;
        ss << std::hex << std::setw(4) << std::setfill('0') << epoch << "-" << seedhash.substr(0, 12) << ".dag";
        auto const epoch_file = GetDataDir() / "dag" / ss.str();

        printf("\nDAG file for epoch %u is \"%s\"", epoch, epoch_file.string().c_str());
        // try to load the DAG from disk
        try {
            std::unique_ptr<dag_t> new_dag(new dag_t(epoch_file.string(), callback));
            ActiveDAG(move(new_dag));
            printf("\nDAG file \"%s\" loaded successfully. \n\n\n", epoch_file.string().c_str());

            egihash::dag_t::data_type data = dag->data();
            for ( int i = 0; i < 16; ++i ) {
                std::cout << "NODE " << std::dec << i << std::hex << " " << data[0][i].hword << std::endl;
            }
            return;
        } catch (hash_exception const & e) {
            printf("\nDAG file \"%s\" not loaded, will be generated instead. Message: %s\n", epoch_file.string().c_str(), e.what());
        }
        // try to generate the DAG
        try {
            std::unique_ptr<dag_t> new_dag(new dag_t(height, callback));
            boost::filesystem::create_directories(epoch_file.parent_path());
            new_dag->save(epoch_file.string());
            ActiveDAG(move(new_dag));
            printf("\nDAG generated successfully. Saved to \"%s\".\n", epoch_file.string().c_str());
        } catch (hash_exception const & e) {
            printf("\nDAG for epoch %u could not be generated: %s", epoch, e.what());
        }
    }
    printf("\nDAG has been initialized already. Use ActiveDAG() to swap.\n");
}
