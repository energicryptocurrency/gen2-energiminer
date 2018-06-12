/*
 * CpuMiner.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include "CpuMiner.h"
#include "common/Log.h"
#include "common/common.h"

using namespace energi;

CpuMiner::CpuMiner(const Plant &plant, int index)
    :Miner("CPU/", plant, index)
{
    // First time init egi hash dag
    // We got to do for every epoch change below
    LoadNrgHashDAG();
}

void CpuMiner::trun()
{
    try {
        while (true) {
            Work work = this->getWork(); // This work is a copy of last assigned work the worker was provided by plant
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
            const uint64_t first_nonce = m_nonceStart.load();
            const uint64_t max_nonce = m_nonceEnd.load();

            work.nNonce = first_nonce;
            uint64_t last_nonce = first_nonce;
            m_newWorkAssigned = false;
            // we dont use mixHash part to calculate hash but fill it later (below)
            do {
                auto hash = GetPOWHash(work);
                if (UintToArith256(hash) < work.hashTarget) {
                    addHashCount(work.nNonce + 1 - last_nonce);
                    cnote << name() << "Submitting block blockhash: " << work.GetHash().ToString() << " height: " << work.nHeight << "nonce: " << work.nNonce;
                    m_plant.submitProof(Solution(work, work.getSecondaryExtraNonce()));
                    break;
                } else {
                    ++work.nNonce;
                }
                // rough guess
                if ( work.nNonce % 10000 == 0 ) {
                    addHashCount(work.nNonce - last_nonce);
                    last_nonce = work.nNonce;
                }
            } while (!m_newWorkAssigned && (work.nNonce < max_nonce || !this->shouldStop()));
            addHashCount(work.nNonce - last_nonce);
        }
    } catch(WorkException &ex) {
        cnote << ex.what();
    }
    catch(...) {
    }
}

