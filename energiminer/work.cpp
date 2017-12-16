/*
 * Work.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include "work.h"
#include <memory>

namespace energi
{
  Work::Work(const Json::Value &gbt, const std::string &coinbase_addr)
  {
    if ( !( gbt.isMember("height") && gbt.isMember("version") && gbt.isMember("previousblockhash") ) )
    {
      throw WorkException("Height or Version or Previous Block Hash not found");
    }

    height                    = gbt["height"].asInt();
    auto version              = gbt["version"].asInt();
    auto transactions         = gbt["transactions"];
    auto coinbasevalue        = gbt["coinbasevalue"].asInt64();
    auto target_hex           = gbt["target"].asString();
    auto bits                 = gbt["bits"];
    auto curtime              = gbt["curtime"];
    auto previousblockhash    = gbt["previousblockhash"];

    auto transactions_data_len = 0;
    for ( auto txn : transactions )
    {
        auto data = txn["data"].asString();
        transactions_data_len += data.size() / 2;
    }


    // Part 1
    // 41 bytes = 4 bytes (version) + 1 bytes ( for one coinbase txncount ) + 32 bytes for txout hash (is null for coinbase) + 4 (index)
    vbyte part1(41, 0); // version, txncount, prevhash, pretxnindex
    setBuffer(part1.data(), 1);
    part1[4] = 1;
    setBuffer(part1.data() + part1.size() - 4, 0xFFFFFFFF);

    // Part 2 -> script length
    vbyte part2(2, 0);// 2 bytes reserved for size

    // Part 3 -> store height
    vbyte part3; // script

    // BIP 34: height in coinbase
    for (int n = height; n > 0; n >>= 8)
    {
        part3.push_back( static_cast<uint8_t>(n & 0xFF) );
    }

    part2[0] = part3.size() + 1;
    part2[1] = part3.size(); //  Not sure whats the purposer here

    // Part 4 sequence (4 bytes)
    vbyte part4(4, 0xFF);

    // Part 5 outputs count ( 1 byte)
    vbyte part5(1, 1); // output count

    // Part 6 coin base gbt 8 bytes -> 64 bit integer
    vbyte part6(8, 0); // gbt of coinbase
    setBuffer(part6.data(), (uint32_t)coinbasevalue);
    setBuffer(part6.data() + 4, (uint32_t)( coinbasevalue >> 32 ));

    // Part 8
    vbyte part8(25, 0);
    // wallet address for coinbase reward
    auto pk_script_size = address_to_script(part8.data(), part8.size(), coinbase_addr.c_str());
    if (!pk_script_size)
    {
        //fprintf(stderr, "invalid address -- '%s'\n", arg);
        throw WorkException("Invalid coinbase address");
    }

    // Part 7 tx out script length
    vbyte part7(1, (uint8_t)pk_script_size); // txout script length

    vbyte part9(4, 0);


    const std::string kCoinbaseSig = "ranjeet miner";
    // TODO coinbaseaux

    vbyte coinbase_txn;
    for( auto part : std::vector<vbyte >{std::move(part1)
                    , std::move(part2)
                    , std::move(part3)
                    , std::move(part4)
                    , std::move(part5)
                    , std::move(part6)
                    , std::move(part7)
                    , std::move(vbyte(part8.begin(), part8.begin() + pk_script_size))
                    , std::move(part9)})
    {
        coinbase_txn.insert(coinbase_txn.end(), part.begin(), part.end());
    }

    //Print()(coinbase_txn.data(), coinbase_txn.size());

    vbyte txc_vi(9, 0);
    auto n = varint_encode(txc_vi.data(), 1 + transactions.size());
    std::unique_ptr<char []> txn_data(new char[2 * (n + coinbase_txn.size() + transactions_data_len) + 1]);
    bin2hex(txn_data.get(), txc_vi.data(), n);
    bin2hex(txn_data.get() + 2 * n, coinbase_txn.data(), coinbase_txn.size());

    //std::vector<uint256> vtxn;
    //uint256 merkle_root = CalcMerkleHash(height, pos);
    // generate merkle root
    using MerkleTreeElement = std::array<uint8_t, 32>; // 32 is hash bytes
    std::vector<MerkleTreeElement> merkle_tree( ( 1 + transactions.size() + 1 ) & 0xFFFFFFFF, MerkleTreeElement{});
    sha256d(merkle_tree[0].data(), coinbase_txn.data(), coinbase_txn.size());

    for (int i = 0; i < int(transactions.size()); ++i)
    {
      auto tx_hex = transactions[i]["data"].asString();
      const int tx_size = tx_hex.size() ? (int) (tx_hex.size() / 2) : 0;
      vbyte tx(tx_size, 0);
      if (!hex2bin(tx.data(), tx_hex.c_str(), tx_size))
      {
          //applog(LOG_ERR, "JSON invalid transactions");
          throw WorkException("Invalid hex2 bin conversion");
      }

      sha256d(merkle_tree[1 + i].data(), tx.data(), tx_size);
      // TODO
      // if (!submit_coinbase)
      //auto diff = txn_data.end() - txn_data.begin();
      std::strcat(txn_data.get(), tx_hex.c_str());
    }

    rawTransactionData.append(txn_data.get());

    n = 1 + transactions.size();

    while (n > 1)
    {
      if (n % 2)
      {
          memcpy(merkle_tree[n].data(), merkle_tree[n-1].data(), 32);
          ++n;
      }

      n /= 2;
      for (int i = 0; i < n; i++)
          sha256d(merkle_tree[i].data(), merkle_tree[2*i].data(), 64);
    }


    // assemble block header
    // Part 1 version
    vuint32 block_header_part1(1, swab32(version));

    // Part 2 prev hash
    vuint32 block_header_part2(8, 0), prevhash_hex(8, 0);
    std::string prevhash = previousblockhash.asString();

    hex2bin(reinterpret_cast<uchar*>(prevhash_hex.data()), prevhash.data(), sizeof(uint32_t) * prevhash_hex.size());
    for (int i = 0; i < 8; i++)
      block_header_part2[7 - i] = le32dec(prevhash_hex.data() + i);

    // Part 3 merkle hash
    vuint32 block_header_part3(8, 0);
    for (int i = 0; i < 8; i++)
      block_header_part3[i] = be32dec((uint32_t *)merkle_tree[0].data() + i);


    for ( auto mn : merkle_tree )
    {
      //Print()(mn);
    }

    // Part 4 current time
    vuint32 block_header_part4(1, swab32(curtime.asInt()));

    // Part 5 bits
    uint32_t bits_parsed = 0;
    hex2bin(reinterpret_cast<uchar*>(&bits_parsed), bits.asString().c_str(), sizeof(bits_parsed));
    vuint32 block_header_part5(1, le32dec(&bits_parsed));

    // Part 6 height -> egihash specific not in Bitcoin
    vuint32 block_header_part6(1, swab32(height));

    // Part 7 nonce
    vuint32 block_header_part7(13, 0);
    block_header_part7[1] = 0x80000000;
    block_header_part7[12] = 0x00000280;

    // Extra dont know why
    vuint32 block_header_part8(16, 0);

    vuint32 target_bin(8, 0);
    hex2bin((uint8_t*)target_bin.data(), target_hex.data(), sizeof(target_bin) * target_bin.size());

    for (int i = 0; i < 8; i++)
      targetBin[7 - i] = be32dec(target_bin.data() + i);

    for( auto part : std::vector<vuint32>{std::move(block_header_part1)
            , std::move(block_header_part2)
            , std::move(block_header_part3)
            , std::move(block_header_part4)
            , std::move(block_header_part5)
            , std::move(block_header_part6)
            , std::move(block_header_part7)
        , std::move(block_header_part8)
    })
    {
      blockHeader.insert(blockHeader.end(), part.begin(), part.end());
    }
  }

  /*
  void serialize_work(const Work &work)
    {
     std::ofstream f ("/var/tmp/gpu_miner.hex", std::ios_base::out | std::ios_base::binary);
     f.write(reinterpret_cast<const char*>(work.data.data()), sizeof(uint32_t) * work.data.size());
     f << "hello" << endl;
     f.write(reinterpret_cast<const char*>(work.target.data()), sizeof(uint32_t) * work.target.size());
     f.write(reinterpret_cast<const char*>(work.rawTransactionData.c_str()), work.rawTransactionData.size());
     f.write(reinterpret_cast<const char*>(&work.height), sizeof(work.height));
    }*/
} /* namespace energi */
