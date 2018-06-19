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

Work::Work(const Json::Value& gbt,
           const std::string& extraNonce, bool)
    : Block(gbt, extraNonce, true)
    , m_extraNonce(extraNonce)
{
    m_jobName = gbt.get((Json::Value::ArrayIndex)0, "").asString();
    hashTarget = arith_uint256().SetCompact(this->nBits);
}

Work::Work(const Json::Value &gbt,
           const std::string &coinbase_addr)
    : Block(gbt, coinbase_addr)
{
    hashTarget = arith_uint256().SetCompact(this->nBits);
}

void Work::incrementExtraNonce()
{
    static uint256 hashPrev;
    if (hashPrev != this->hashPrevBlock) {
        m_secondaryExtraNonce = 0;
        hashPrev = this->hashPrevBlock;
    }
    ++m_secondaryExtraNonce;
    CMutableTransaction txCoinbase(this->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << this->nHeight << CScriptNum(m_secondaryExtraNonce)) + COINBASE_FLAGS;

   this->vtx[0] = txCoinbase;
   this->hashMerkleRoot = BlockMerkleRoot(*this);
}

void Work::updateTimestamp()
{
    nTime = std::chrono::seconds(std::time(NULL)).count();
}

} //! namespace energi
