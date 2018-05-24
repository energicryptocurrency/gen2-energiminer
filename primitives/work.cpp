/*
 * Work.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include <chrono>
#include <memory>
#include "base58.h"
#include "work.h"

namespace energi {

CScript COINBASE_FLAGS;

Work::Work(const Json::Value &gbt,
           const std::string &coinbase_addr,
           const std::string& coinbase1,
           const std::string& coinbase2,
           const std::string& job,
           const std::string& extraNonce)
    : Block(gbt, coinbase_addr, coinbase1, coinbase2, extraNonce)
    , m_jobName(job)
    , m_extraNonce(extraNonce)
{
    hashTarget = arith_uint256().SetCompact(this->nBits);
}

void Work::incrementExtraNonce(unsigned int& nExtraNonce)
{
    static uint256 hashPrev;
    if (hashPrev != this->hashPrevBlock) {
        nExtraNonce = 0;
        hashPrev = this->hashPrevBlock;
    }
    ++nExtraNonce;
    CMutableTransaction txCoinbase(this->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << this->nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;

   this->vtx[0] = txCoinbase;
   this->hashMerkleRoot = BlockMerkleRoot(*this);
}

void Work::incrementExtraNonce()
{
    unsigned int extraNonce=0;
    incrementExtraNonce(extraNonce);
}

void Work::updateTimestamp()
{
    nTime = std::chrono::seconds(std::time(NULL)).count();
}

} //! namespace energi