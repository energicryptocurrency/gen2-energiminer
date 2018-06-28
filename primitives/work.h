/*
 * work.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_WORK_H_
#define ENERGIMINER_WORK_H_

#include "common/common.h"
#include "merkle.h"
#include "uint256.h"
#include "arith_uint256.h"
#include "block.h"

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
        : Block()
    {}

    Work(Work &&) = default; // -> Blank work for comparisons
    Work& operator=(Work &&) = default;
    Work(const Work &) = default; // -> Blank work for comparisons
    Work(const Json::Value& gbt,
         const std::string& extraNonce, bool);

    Work(const Json::Value& gbt,
         const std::string& coinbase_addr); // -> coinbase to transfer miners reward

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
        SetNull();
        m_jobName = std::string();
        m_extraNonce = std::string();
    }

    bool isValid() const
    {
        return this->nHeight > 0;
    }

    inline const std::string& getJobName() const
    {
        return m_jobName;
    }

    void incrementExtraNonce();

    void updateTimestamp();

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(*(Block*)this);
    }

    inline uint64_t getExtraNonce() const
    {
        return std::strtol(m_extraNonce.c_str(), nullptr, 16);
    }

    inline uint32_t getSecondaryExtraNonce() const
    {
        return m_secondaryExtraNonce;
    }

    void setExtraNonce(const std::string& exNonce)
    {
        m_extraNonce = exNonce;
    }


    //!TODO keep only this part
    uint64_t       startNonce = 0;
    int            exSizeBits = -1;
    uint32_t       m_secondaryExtraNonce = 0;
    std::string    m_jobName;
    std::string    m_extraNonce;
    arith_uint256  hashTarget;

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
