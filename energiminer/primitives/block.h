#pragma once

#include <json/json.h>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <vector>

#include "energiminer/transaction.h"
#include "serialize.h"
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
    std::vector<coinbase_tx> vtx;

    Block()
    {
        SetNull();
    }

    Block(const Json::Value &gbt)
        : BlockHeader(gbt)
    {
    }

    Block(const BlockHeader& header)
    {
        SetNull();
        *((BlockHeader*)this) = header;
    }

    template<typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(*(BlockHeader*)this);
        //READWRITE(vtx);
    }

    void SetNull()
    {
        BlockHeader::SetNull();
        vtx.clear();
    }
};

#pragma pack(push, 1)

//*
//*   The Keccak-256 hash of this structure is used as input for egihash
//*   It is a truncated block header with a deterministic encoding
//*   All integer values are little endian
//*   Hashes are the nul-terminated hex encoded representation as if ToString() was called

// Bytes => 4 + 65 + 65 + 4 + 4 + 4 -> 130 + 16 -> 146 bytes
struct CBlockHeaderTruncatedLE
{
    int32_t nVersion;
    char hashPrevBlock[65];
    char hashMerkleRoot[65];
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nHeight;

    CBlockHeaderTruncatedLE(const void* data)
        : nVersion(htole32(*reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(data) + 0)))
        , hashPrevBlock{0}
        , hashMerkleRoot{0}
        , nTime(htole32(*reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(data) + 4 + 32 + 32 )))
        , nBits(htole32(*reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(data) + 4 + 32 + 32 + 4 )))
        , nHeight(htole32(*reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(data) + 4 + 32 + 32 + 4 + 4 )))
    {
        auto inputHashPrevBlock = reinterpret_cast<const uint8_t*>(data) + 4;
        auto prevHash = GetHex(inputHashPrevBlock, 32);
        std::memcpy(hashPrevBlock, prevHash.c_str(), (std::min)(prevHash.size(), sizeof(hashPrevBlock)));

        auto inputMerkleHashPrevBlock = reinterpret_cast<const uint8_t*>(data) + 36;
        auto merkleRoot = GetHex(inputMerkleHashPrevBlock, 32);
        std::memcpy(hashMerkleRoot, merkleRoot.c_str(), (std::min)(merkleRoot.size(), sizeof(hashMerkleRoot)));
    }

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

struct CBlockHeaderFullLE : public CBlockHeaderTruncatedLE
{
    uint32_t nNonce;
    char hashMix[65];

    CBlockHeaderFullLE(const void* data, uint32_t nonce, const uint8_t* hashMix_)
        : CBlockHeaderTruncatedLE(data)
          , nNonce(nonce)
          , hashMix{0}
    {
        auto mixString = GetHex(hashMix_, 32);
        /*cdebug << "NONCE:" << nNonce;
          cdebug << "GET HEX HASH:" << mixString;*/
        std::memcpy(hashMix, mixString.c_str(), (std::min)(mixString.size(), sizeof(hashMix)));
    }
};
static_assert(sizeof(CBlockHeaderFullLE) == 215, "CBlockHeaderFullLE has incorrect size");
#pragma pack(pop)

} // namespace energi
