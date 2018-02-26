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
    :Miner("cpu", plant, index)
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

            uint32_t _ALIGN(128) hash[8];
            uint32_t _ALIGN(128) endiandata[29];
            uint32_t *pdata = work.blockHeader.data();
            uint32_t *ptarget = work.targetBin.data();

            const uint32_t Htarg = ptarget[7];
            uint32_t nonce = first_nonce;
            uint32_t last_nonce = first_nonce;
            // we dont use mixHash part to calculate hash but fill it later (below)
            for (int k=0; k < 20; k++) {
                be32enc(&endiandata[k], pdata[k]);
            }
            do {
                auto hash_res = GetPOWHash(work.height, nonce, endiandata);
                memcpy(hash, hash_res.value.b, sizeof(hash_res.value));
                uint32_t arr[8] = {0};
                memcpy(arr, hash_res.mixhash.b, sizeof(hash_res.mixhash));
                for (int i = 0; i < 8; i++) {
                    pdata[i + 20] = be32dec(&arr[i]);
                }
                if (hash[7] <= Htarg && fulltest(hash, ptarget)) {
                    auto nonceForHash = be32dec(&nonce);
                    pdata[28] = nonceForHash;
                    addHashCount(nonce + 1 - last_nonce);

                    Solution solution(work, nonce);
                    plant_.submit(solution);
                    return;
                }
                nonce++;
                // rough guess
                if ( nonce % 10000 == 0 ) {
                    addHashCount(nonce - last_nonce);
                    last_nonce = nonce;
                }
            } while (nonce < max_nonce || !this->shouldStop() );

            pdata[28] = be32dec(&nonce);
            addHashCount(nonce - last_nonce);
        }
    } catch(WorkException &ex) {
        cnote << ex.what();
    }
    catch(...) {
    }
}

