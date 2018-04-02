#pragma once

#include <json/json.h>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <vector>

#include "energiminer/primitives/transaction.h"
#include "energiminer/common/utilstrencodings.h"
#include "energiminer/common/serialize.h"
#include "uint256.h"

namespace energi {

struct BlockHeader
{
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nHeight;
    uint256 hashMix;
    uint32_t nNonce;

    BlockHeader()
    {
        SetNull();
    }

    BlockHeader(const Json::Value& gbt)
    {
        nVersion         = gbt["version"].asInt();
        hashPrevBlock.SetHex(gbt["previousblockhash"].asString());
        hashMerkleRoot.SetNull();
        nTime            = gbt["curtime"].asInt();
        std::string bits = gbt["bits"].asString();
        nBits = std::strtol(gbt["bits"].asString().c_str(), nullptr, 16);
        nHeight          = gbt["height"].asInt();
        hashMix.SetNull();
        nNonce = 0;
    }

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nHeight);
        READWRITE(hashMix);
        READWRITE(nNonce);
    }

    void SetNull()
    {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nHeight = 0;
        hashMix.SetNull();
        nNonce = 0;
    }
};

struct Block : public BlockHeader
{
    std::vector<CTransaction> vtx;

    CTxOut txoutBackbone; // Energibackbone payment
    CTxOut txoutMasternode; // masternode payment
    std::vector<CTxOut> voutSuperblock; //superblock payment

    Block()
    {
        SetNull();
    }

    Block(const Json::Value& gbt, const std::string& coinbaseAddress)
        : BlockHeader(gbt)
    {
        if ( !( gbt.isMember("height") && gbt.isMember("version") && gbt.isMember("previousblockhash") ) ) {
            throw WorkException("Height or Version or Previous Block Hash not found");
        }
        auto coinbaseValue        = gbt["coinbasevalue"].asInt64();
        CKeyID keyID;
        if (!CBitcoinAddress(coinbaseAddress).GetKeyID(keyID)) {
            throw WorkException("Could not get KeyID for address");
        }
        CScript script = GetScriptForDestination(keyID);
        CTxOut out = CTxOut(coinbaseValue, script);
        fillTransactions(gbt);
    }

    Block(const BlockHeader& header)
    {
        SetNull();
        *((BlockHeader*)this) = header;
    }

    void fillTransactions(const Json::Value& gbt)
    {
        // masternode payment
        bool const masternode_payments_started = gbt["masternode_payments_started"].asBool();
        //bool const masternode_payments_enforced = gbt["masternode_payments_enforced"].asBool(); // not used currently

        // superblock payments
        bool const superblocks_enabled = gbt["superblocks_enabled"].asBool();
        auto const superblock_proposals = gbt["superblock"];
        txoutBackbone = outTransaction(gbt["backbone"]);
        if (masternode_payments_started) {
            txoutMasternode = outTransaction(gbt["masternode"]);
        }
        if (superblocks_enabled) {
            const auto superblock = gbt["superblock"];
            for (const auto& proposal_payee : superblock) {
                voutSuperblock.push_back(outTransaction(proposal_payee));
            }
        }
        auto transactions = gbt["transactions"];
        for (const auto& txn : transactions) {
            CTransaction trans;
            DecodeHexTx(trans, txn["data"].asString());
            vtx.push_back(trans);
        }
    }

    CTxOut outTransaction(const Json::Value& json) const
    {
        if (json.isMember("payee") && json.isMember("script") && json.isMember("amount")) {
            std::string scriptStr = json["script"].asString();
            if (!IsHex(scriptStr)) {
                throw WorkException("Cannot decode script");
            }
            auto data = ParseHex(scriptStr);
            CScript transScript(data.begin(), data.end());
            CTxOut trans(json["amount"].asUInt(), transScript);
            return trans;
        }
        return CTxOut();
    }

    template<typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(*(BlockHeader*)this);
        READWRITE(vtx);
    }

    void SetNull()
    {
        BlockHeader::SetNull();
        vtx.clear();
        txoutBackbone = CTxOut();
        txoutMasternode = CTxOut();
        voutSuperblock.clear();
    }
};

#pragma pack(push, 1)

struct CBlockHeaderTruncatedLE
{
    int32_t nVersion;
    char hashPrevBlock[65];
    char hashMerkleRoot[65];
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nHeight;

    CBlockHeaderTruncatedLE(const BlockHeader& header)
        : nVersion(htole32(header.nVersion))
        , hashPrevBlock{0}
        , hashMerkleRoot{0}
        , nTime(htole32(header.nTime))
        , nBits(htole32(header.nBits))
        , nHeight(htole32(header.nHeight))
    {
        auto prevHash = header.hashPrevBlock.ToString();
        memcpy(hashPrevBlock, prevHash.c_str(), (std::min)(prevHash.size(), sizeof(hashPrevBlock)));

        auto merkleRoot = header.hashMerkleRoot.ToString();
        memcpy(hashMerkleRoot, merkleRoot.c_str(), (std::min)(merkleRoot.size(), sizeof(hashMerkleRoot)));
    }
};

static_assert(sizeof(CBlockHeaderTruncatedLE) == 146, "CBlockHeaderTruncatedLE has incorrect size");
#pragma pack(pop)

} // namespace energi
