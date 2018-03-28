/*
 * CpuMiner.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include "CpuMiner.h"
#include "energiminer/Log.h"
#include "energiminer/common.h"

using namespace energi;

CpuMiner::CpuMiner(const Plant &plant, int index)
    :Miner("CPU/", plant, index)
{
}

void CpuMiner::trun()
{
    try {
        while (true) {
            Work work = this->work(); // This work is a copy of last assigned work the worker was provided by plant
            if ( !work.isValid() ) {
                cnote << "No work received. Pause for 1 s.";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if ( this->shouldStop() ) {
                    break;
                }
                continue;
            } else {
                //cnote << "Valid work.";
            }
            const uint32_t first_nonce = nonceStart_.load();
            const uint32_t max_nonce = nonceEnd_.load();

            uint32_t nonce = first_nonce;
            uint32_t last_nonce = first_nonce;
            // we dont use mixHash part to calculate hash but fill it later (below)
            do {
                auto hash = GetPOWHash(work);
                if (UintToArith256(hash) < work.hashTarget) {
                    addHashCount(nonce + 1 - last_nonce);

                    Solution solution(work, nonce, work.hashMix);
                    plant_.submit(solution);
                    return;
                }
                ++nonce;
                // rough guess
                if ( nonce % 10000 == 0 ) {
                    addHashCount(nonce - last_nonce);
                    last_nonce = nonce;
                }
            } while (nonce < max_nonce || !this->shouldStop() );
            addHashCount(nonce - last_nonce);
        }
    } catch(WorkException &ex) {
        cnote << ex.what();
    }
    catch(...) {
    }
}

