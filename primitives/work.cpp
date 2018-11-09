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
#include "extranoncesingleton.h"

namespace energi {

Work::Work(const Json::Value& stratum,
           const std::string& extraNonce1, const arith_uint256& hashTarget)
    : Block(stratum, extraNonce1, true)
    , m_extraNonce1(extraNonce1)
    , hashTarget(hashTarget)
{
    m_jobName = stratum.get(Json::Value::ArrayIndex(0), "").asString();
}

Work::Work(const Json::Value &gbt,
           const std::string &coinbase_addr)
    : Block(gbt, coinbase_addr)
{
    hashTarget = arith_uint256().SetCompact(this->nBits);
}

void Work::updateExtraNonce()
{
    auto &noncegen = ExtraNonceSingleton::getInstance();
    noncegen.generateExtraNonce();
    m_extraNonce2 = noncegen.getExtraNonce();
    mutateCoinbase(m_extraNonce1, getExtraNonce2());
}

void Work::updateTimestamp()
{
    nTime = std::chrono::seconds(std::time(NULL)).count();
}

std::string Work::getBlockTransaction() const
{
    CDataStream stream(SER_NETWORK, 70208);
    stream << vtx[0];
    return HexStr(stream);
}

void Work::mutateCoinbase(const std::string &extraNonce1, const std::string &extraNonce2) {
    if (stratum_coinbase1.empty()) {
        CMutableTransaction coinbaseTx(this->vtx[0]);
        coinbaseTx.vin[0].scriptSig = CScript()
            << this->nHeight
            << ParseHex(extraNonce1 + extraNonce2);
        vtx[0] = coinbaseTx;
    } else {
        std::string hexData = stratum_coinbase1 + extraNonce1 + extraNonce2 + stratum_coinbase2;
        CTransaction coinbaseTx;
        DecodeHexTx(coinbaseTx, hexData);

        vtx[0] = coinbaseTx;
        vtx[0].UpdateHash();
    }

    hashMerkleRoot = BlockMerkleRoot(*this);
}

} //! namespace energi
