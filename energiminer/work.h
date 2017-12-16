/*
 * work.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_WORK_H_
#define ENERGIMINER_WORK_H_

#include "energiminer/common.h"

#include <limits>
#include <cstdint>
#include <cstring>
#include <string>
#include <jsoncpp/json/json.h>


namespace energi
{
  // Work receives getblocktemplate input string, json
  // After parsing it builds the block to be mined and passes to miners
  // after finding POW, creates solution
  // Block -> block header + raw transaction data ( txncount + raw transactions )
  struct Work
  {
    Work()
    {}

    Work(Work &&) = default; // -> Blank work for comparisons
    Work& operator=(Work &&) = default;
    Work(const Work &) = default; // -> Blank work for comparisons
    Work(const Json::Value &gbt, const std::string &coinbase_addr); // -> coinbase to transfer miners reward
    Work& operator=(const Work &) = default;

    bool operator==(const Work& other) const
    {
      return std::memcmp(this->targetBin.data(), other.targetBin.data(), sizeof(target) ) == 0 && this->height == other.height;
    }

    bool operator!=(const Work& other) const
    {
        return !operator==(other);
    }

    bool isValid() const
    {
      return this->height > 0;
    }

    uint32_t                height  = 0;
    vuint32                 blockHeader;
    target                  targetBin;
    std::string             rawTransactionData;
  };


  struct SimulatedWork : Work
  {
  };

} /* namespace energi */

#endif /* ENERGIMINER_WORK_H_ */
