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
}

void CpuMiner::trun()
{
    uint64_t startNonce = 0;
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

            if (!m_dagLoaded || ((work.nHeight / nrghash::constants::EPOCH_LENGTH) != (m_lastHeight / nrghash::constants::EPOCH_LENGTH))) {
                static std::mutex mtx;
                std::lock_guard<std::mutex> lock(mtx);
                LoadNrgHashDAG(work.nHeight);
                cnote << "End initialising";
                m_dagLoaded = true;
            }
            m_lastHeight = work.nHeight;

            startNonce = m_plant.getStartNonce(work, m_index);

            work.nNonce = startNonce;
            uint64_t lastNonce = startNonce;
            m_newWorkAssigned = false;
            // we dont use mixHash part to calculate hash but fill it later (below)
            do {
                auto hash = GetPOWHash(work);
                if (UintToArith256(hash) < work.hashTarget) {
                    updateHashRate(work.nNonce + 1 - lastNonce);
                    Solution sol = Solution(work, work.getSecondaryExtraNonce());
                    cnote << name() << "Submitting block blockhash: " << work.GetHash().ToString() << " height: " << work.nHeight << "nonce: " << work.nNonce;
                    m_plant.submitProof(Solution(work, work.getSecondaryExtraNonce()));
                    ++work.nNonce;
                    break;
                } else {
                    ++work.nNonce;
                }
                // rough guess
                if ( work.nNonce % 10000 == 0 ) {
                    updateHashRate(work.nNonce - lastNonce);
                    lastNonce = work.nNonce;
                }
            } while (!m_newWorkAssigned && !this->shouldStop());
            updateHashRate(work.nNonce - lastNonce);
        }
    } catch(WorkException &ex) {
        cnote << ex.what();
    } catch(...) {
    }
}

