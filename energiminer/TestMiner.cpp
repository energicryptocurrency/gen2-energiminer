/*
 * TestMiner.cpp
 *
 *  Created on: Dec 14, 2017
 *      Author: ranjeet
 */

#include "TestMiner.h"

#include "primitives/solution.h"

namespace energi
{
  TestMiner::TestMiner(const Plant& plant, unsigned index)
  :Miner("test", plant, index)
  {

  }

  void TestMiner::trun()
  {
    try
    {
      int maxLoop = 10;
      while (--maxLoop >= 0)
      {
        int kMaxCounter = 100; // with 5000,000/10 milli -> 1 sec -> 500,000,000
        uint64_t hashes = 0;
        for(int i = 0; i < kMaxCounter; ++i, hashes += 5000000)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        updateHashRate(hashes);

        if ( maxLoop == 0 ) {
          Solution solution;
          m_plant.submitProof(solution);
        }

        if (shouldStop()) {
          break;
        }
      }
    }
    catch(...)
    {

    }
  }

} /* namespace energi */
