/*
 * work.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_WORK_H_
#define ENERGIMINER_WORK_H_

#include "energiminer/common/common.h"
#include "energiminer/primitives/serialize.h"
#include "energiminer/primitives/uint256.h"
#include "energiminer/primitives/arith_uint256.h"
#include "energiminer/primitives/block.h"

#include <limits>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>


namespace energi
{
// Work receives getblocktemplate input string, json
// After parsing it builds the block to be mined and passes to miners
// after finding POW, creates solution
// Block -> block header + raw transaction data ( txncount + raw transactions )
struct Work : public Block
{
    Work()
    {}

    Work(Work &&) = default; // -> Blank work for comparisons
    Work& operator=(Work &&) = default;
    Work(const Work &) = default; // -> Blank work for comparisons
    Work(const Json::Value &gbt, const std::string &coinbase_addr, const std::string& job = std::string()); // -> coinbase to transfer miners reward
    Work& operator=(const Work &) = default;

    bool operator==(const Work& other) const
    {
        return (hashPrevBlock == other.hashPrevBlock) && (nHeight == other.nHeight);
    }

    bool operator!=(const Work& other) const
    {
        return !operator==(other);
    }

    void reset()
    {
        height = 0;
        SetNull();
    }

    bool isValid() const
    {
        return this->height > 0;
    }

    inline const std::string& getJobName() const
    {
        return m_jobName;
    }

    uint32_t                height  = 0;
    uint32_t                bitsNum = 0;
    vuint32                 blockHeader;
    //std::string             rawTransactionData;


    //!TODO keep only this part
    std::string             m_jobName;
    arith_uint256           hashTarget;

    std::string ToString() const
    {
        std::stringstream ss;
        ss << "Height: " << this->nNonce << " "
           << "Bits: "   << this->nBits << " "
           << "Target: " << this->hashTarget.ToString()
           << "PrevBlockHash: " << this->hashPrevBlock.ToString();
        return ss.str();
    }
};

struct SimulatedWork : Work
{
};

} /* namespace energi */

#endif /* ENERGIMINER_WORK_H_ */
