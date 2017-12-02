/*
 * minercommon.cpp
 *
 *  Created on: Nov 22, 2017
 *      Author: ranjeet
 */

#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <random>
#include <cstdint>
#include <cstring>
#include <thread>
#include <memory>
#include <mutex>
#include <atomic>

#include <cassert>
#include "minercommon.h"
#include <condition_variable>

#include "egihash/egihash.h"

#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <memory>
#include <sstream>
#include <iomanip>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>



using namespace std;

extern "C" void sha256_init(uint32_t *state);
extern "C" void sha256_transform(uint32_t *state, const uint32_t *block, int swap);
extern "C" void sha256d(unsigned char *hash, const unsigned char *data, int len);



#define _ALIGN(x) __attribute__ ((aligned(x)))

bool fulltest(const uint32_t *hash, const uint32_t *target)
{
	int i;
	bool rc = true;

	for (i = 7; i >= 0; i--) {
		if (hash[i] > target[i]) {
			rc = false;
			break;
		}
		if (hash[i] < target[i]) {
			rc = true;
			break;
		}
	}

	/*if (opt_debug) {
		uint32_t hash_be[8], target_be[8];
		char hash_str[65], target_str[65];

		for (i = 0; i < 8; i++) {
			be32enc(hash_be + i, hash[7 - i]);
			be32enc(target_be + i, target[7 - i]);
		}
		bin2hex(hash_str, (unsigned char *)hash_be, 32);
		bin2hex(target_str, (unsigned char *)target_be, 32);

		applog(LOG_DEBUG, "DEBUG: %s\nHash:   %s\nTarget: %s",
			rc ? "hash <= target"
			   : "hash > target (false positive)",
			hash_str,
			target_str);
	}
*/
	return rc;
}

static inline bool is_windows(void) {
#ifdef WIN32
	return 1;
#else
	return 0;
#endif
}

#if ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define WANT_BUILTIN_BSWAP
#else
#define bswap_32(x) ((((x) << 24) & 0xff000000u) | (((x) << 8) & 0x00ff0000u) \
                   | (((x) >> 8) & 0x0000ff00u) | (((x) >> 24) & 0x000000ffu))
#endif

static inline uint32_t swab32(uint32_t v)
{
#ifdef WANT_BUILTIN_BSWAP
	return __builtin_bswap32(v);
#else
	return bswap_32(v);
#endif
}

#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif

typedef unsigned char uchar;

#if !HAVE_DECL_BE32DEC
static inline uint32_t be32dec(const void *pp)
{
	const uint8_t *p = (uint8_t const *)pp;
	return ((uint32_t)(p[3]) + ((uint32_t)(p[2]) << 8) +
	    ((uint32_t)(p[1]) << 16) + ((uint32_t)(p[0]) << 24));
}
#endif

#if !HAVE_DECL_LE32DEC
static inline uint32_t le32dec(const void *pp)
{
	const uint8_t *p = (uint8_t const *)pp;
	return ((uint32_t)(p[0]) + ((uint32_t)(p[1]) << 8) +
	    ((uint32_t)(p[2]) << 16) + ((uint32_t)(p[3]) << 24));
}
#endif

#if !HAVE_DECL_BE32ENC
static inline void be32enc(void *pp, uint32_t x)
{
	uint8_t *p = (uint8_t *)pp;
	p[3] = x & 0xff;
	p[2] = (x >> 8) & 0xff;
	p[1] = (x >> 16) & 0xff;
	p[0] = (x >> 24) & 0xff;
}
#endif

#if !HAVE_DECL_LE32ENC
static inline void le32enc(void *pp, uint32_t x)
{
	uint8_t *p = (uint8_t *)pp;
	p[0] = x & 0xff;
	p[1] = (x >> 8) & 0xff;
	p[2] = (x >> 16) & 0xff;
	p[3] = (x >> 24) & 0xff;
}
#endif

#if !HAVE_DECL_LE16DEC
static inline uint16_t le16dec(const void *pp)
{
	const uint8_t *p = (uint8_t const *)pp;
	return ((uint16_t)(p[0]) + ((uint16_t)(p[1]) << 8));
}
#endif

#if !HAVE_DECL_LE16ENC
static inline void le16enc(void *pp, uint16_t x)
{
	uint8_t *p = (uint8_t *)pp;
	p[0] = x & 0xff;
	p[1] = (x >> 8) & 0xff;
}
#endif






std::unique_ptr<egihash::dag_t> const & ActiveDAG(std::unique_ptr<egihash::dag_t> next_dag  = std::unique_ptr<egihash::dag_t>())
{
    using namespace std;

    static boost::mutex m;
    boost::lock_guard<boost::mutex> lock(m);
    static unique_ptr<egihash::dag_t> active; // only keep one DAG in memory at once

    // if we have a next_dag swap it
    if (next_dag)
    {
        active.swap(next_dag);
    }

    // unload the previous dag
    if (next_dag)
    {
        next_dag->unload();
        next_dag.reset();
    }

    return active;
}


std::string GetHex(const uint8_t* data, unsigned int size)
{
    std::string psz(size * 2 + 1, '\0');
    for (unsigned int i = 0; i < size; i++)
        sprintf(const_cast<char*>(psz.data()) + i * 2, "%02x", data[size - i - 1]);
    return std::string(const_cast<char*>(psz.data()), const_cast<char*>(psz.data()) + size * 2);
}


#pragma pack(push, 1)

//*
//*   The Keccak-256 hash of this structure is used as input for egihash
//*   It is a truncated block header with a deterministic encoding
//*   All integer values are little endian
//*   Hashes are the nul-terminated hex encoded representation as if ToString() was called

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
		memcpy(hashPrevBlock, prevHash.c_str(), (std::min)(prevHash.size(), sizeof(hashPrevBlock)));

		auto inputMerkleHashPrevBlock = reinterpret_cast<const uint8_t*>(data) + 36;
		auto merkleRoot = GetHex(inputMerkleHashPrevBlock, 32);
		memcpy(hashMerkleRoot, merkleRoot.c_str(), (std::min)(merkleRoot.size(), sizeof(hashMerkleRoot)));
	}
};
#pragma pack(pop)



boost::filesystem::path GetDataDir()
{
    namespace fs = boost::filesystem;
	return fs::path("/home/ranjeet/.energicore/regtest/regtest");
}




void InitDAG(egihash::progress_callback_type callback)
{
	using namespace egihash;

	auto const & dag = ActiveDAG();
	if (!dag)
	{
		auto const height = 0;// TODO (max)(GetHeight(), 0);
		auto const epoch = height / constants::EPOCH_LENGTH;
		auto const & seedhash = seedhash_to_filename(get_seedhash(height));
		stringstream ss;
		ss << hex << setw(4) << setfill('0') << epoch << "-" << seedhash.substr(0, 12) << ".dag";
		auto const epoch_file = GetDataDir() / "dag" / ss.str();

		printf("DAG file for epoch %u is \"%s\"", epoch, epoch_file.string().c_str());
		// try to load the DAG from disk
		try
		{
			unique_ptr<dag_t> new_dag(new dag_t(epoch_file.string(), callback));
			ActiveDAG(move(new_dag));
			printf("DAG file \"%s\" loaded successfully.", epoch_file.string().c_str());
			return;
		}
		catch (hash_exception const & e)
		{
			printf("DAG file \"%s\" not loaded, will be generated instead. Message: %s", epoch_file.string().c_str(), e.what());
		}

		// try to generate the DAG
		try
		{
			unique_ptr<dag_t> new_dag(new dag_t(height, callback));
			boost::filesystem::create_directories(epoch_file.parent_path());
			new_dag->save(epoch_file.string());
			ActiveDAG(move(new_dag));
			printf("DAG generated successfully. Saved to \"%s\".", epoch_file.string().c_str());
		}
		catch (hash_exception const & e)
		{
			printf("DAG for epoch %u could not be generated: %s", epoch, e.what());
		}
	}
	printf("DAG has been initialized already. Use ActiveDAG() to swap.");
}


bool InitEgiHashDag()
{
	// initialize the DAG
	InitDAG([](::std::size_t step, ::std::size_t max, int phase) -> bool
	{
			std::stringstream ss;
			ss << std::fixed << std::setprecision(2)
			<< static_cast<double>(step) / static_cast<double>(max) * 100.0 << "%"
			<< std::setfill(' ') << std::setw(80);

			auto progress_handler = [&](std::string const &msg)
			{
				std::cout << "\r" << msg;
			};

		switch(phase)
		{
			case egihash::cache_seeding:
					progress_handler("Seeding cache ... ");
				break;
			case egihash::cache_generation:
					progress_handler("Generating cache ... ");
				break;
			case egihash::cache_saving:
					progress_handler("Saving cache ... ");
				break;
			case egihash::cache_loading:
					progress_handler("Loading cache ... ");
				break;
			case egihash::dag_generation:
					progress_handler("Generating Dag ... ");
				break;
			case egihash::dag_saving:
					progress_handler("Saving Dag ... ");
				break;
			case egihash::dag_loading:
					progress_handler("Loading Dag ... ");
				break;
			default:
				break;
		}

		auto progress = ss.str();
		std::cout << progress << std::flush;

		return true;
	});

	return true;
}

egihash::h256_t egihash_calc(uint32_t height, uint32_t nonce, const void *input)
{
	CBlockHeaderTruncatedLE truncatedBlockHeader(input);
	egihash::h256_t headerHash(&truncatedBlockHeader, sizeof(truncatedBlockHeader));
	egihash::result_t ret;
	// if we have a DAG loaded, use it
	auto const & dag = ActiveDAG();

	if (dag && ((height / egihash::constants::EPOCH_LENGTH) == dag->epoch()))
	{
		return egihash::full::hash(*dag, headerHash, nonce).value;
	}

    // otherwise all we can do is generate a light hash
    // TODO: pre-load caches and seed hashes
    return egihash::light::hash(egihash::cache_t(height, egihash::get_seedhash(height)), headerHash, nonce).value;
}





using namespace std;

namespace energi
{
    static const char b58digits[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";


    static bool b58dec(unsigned char *bin, size_t binsz, const char *b58)
    {
        size_t i, j;
        uint64_t t;
        uint32_t c;
        uint32_t *outi;
        size_t outisz = (binsz + 3) / 4;
        int rem = binsz % 4;
        uint32_t remmask = 0xffffffff << (8 * rem);
        size_t b58sz = strlen(b58);
        bool rc = false;

        outi = (uint32_t *) calloc(outisz, sizeof(*outi));

        for (i = 0; i < b58sz; ++i) {
            for (c = 0; b58digits[c] != b58[i]; c++)
                if (!b58digits[c])
                    goto out;
            for (j = outisz; j--; ) {
                t = (uint64_t)outi[j] * 58 + c;
                c = t >> 32;
                outi[j] = t & 0xffffffff;
            }
            if (c || outi[0] & remmask)
                goto out;
        }

        j = 0;
        switch (rem) {
            case 3:
                *(bin++) = (outi[0] >> 16) & 0xff;
            case 2:
                *(bin++) = (outi[0] >> 8) & 0xff;
            case 1:
                *(bin++) = outi[0] & 0xff;
                ++j;
            default:
                break;
        }
        for (; j < outisz; ++j) {
            be32enc((uint32_t *)bin, outi[j]);
            bin += sizeof(uint32_t);
        }

        rc = true;
        out:
        free(outi);
        return rc;
    }

    static int b58check(unsigned char *bin, size_t binsz, const char *b58)
    {
        unsigned char buf[32];
        int i;

        sha256d(buf, bin, (int) (binsz - 4));
        if (memcmp(&bin[binsz - 4], buf, 4))
            return -1;

        /* Check number of zeros is correct AFTER verifying checksum
         * (to avoid possibility of accessing the string beyond the end) */
        for (i = 0; bin[i] == '\0' && b58[i] == '1'; ++i);
        if (bin[i] == '\0' || b58[i] == '1')
            return -3;

        return bin[0];
    }


    size_t address_to_script(unsigned char *out, size_t outsz, const char *addr)
    {
        unsigned char addrbin[25];
        int addrver;
        size_t rv;

        if (!b58dec(addrbin, sizeof(addrbin), addr))
            return 0;
        addrver = b58check(addrbin, sizeof(addrbin), addr);
        if (addrver < 0)
            return 0;
        switch (addrver) {
            case 5:    /* Bitcoin script hash */
            case 196:  /* Testnet script hash */
                if (outsz < (rv = 23))
                    return rv;
                out[ 0] = 0xa9;  /* OP_HASH160 */
                out[ 1] = 0x14;  /* push 20 bytes */
                memcpy(&out[2], &addrbin[1], 20);
                out[22] = 0x87;  /* OP_EQUAL */
                return rv;
            default:
                if (outsz < (rv = 25))
                    return rv;
                out[ 0] = 0x76;  /* OP_DUP */
                out[ 1] = 0xa9;  /* OP_HASH160 */
                out[ 2] = 0x14;  /* push 20 bytes */
                memcpy(&out[3], &addrbin[1], 20);
                out[23] = 0x88;  /* OP_EQUALVERIFY */
                out[24] = 0xac;  /* OP_CHECKSIG */
                return rv;
        }
    }

    int varint_encode(unsigned char *p, uint64_t n)
    {
        int i;
        if (n < 0xfd) {
            p[0] = (uint8_t) n;
            return 1;
        }
        if (n <= 0xffff) {
            p[0] = 0xfd;
            p[1] = n & 0xff;
            p[2] = (uint8_t) (n >> 8);
            return 3;
        }
        if (n <= 0xffffffff) {
            p[0] = 0xfe;
            for (i = 1; i < 5; i++) {
                p[i] = n & 0xff;
                n >>= 8;
            }
            return 5;
        }
        p[0] = 0xff;
        for (i = 1; i < 9; i++) {
            p[i] = n & 0xff;
            n >>= 8;
        }
        return 9;
    }


    void bin2hex(char *s, const unsigned char *p, size_t len)
    {
        for (size_t i = 0; i < len; i++)
            sprintf(s + (i * 2), "%02x", (unsigned int) p[i]);
    }

    char *abin2hex(const unsigned char *p, size_t len)
    {
        char *s = (char*) malloc((len * 2) + 1);
        if (!s)
            return NULL;
        bin2hex(s, p, len);
        return s;
    }

    bool hex2bin(unsigned char *p, const char *hexstr, size_t len)
    {
        char hex_byte[3];
        char *ep;

        hex_byte[2] = '\0';

        while (*hexstr && len) {
            if (!hexstr[1]) {
                //applog(LOG_ERR, "hex2bin str truncated");
                return false;
            }
            hex_byte[0] = hexstr[0];
            hex_byte[1] = hexstr[1];
            *p = (unsigned char) strtol(hex_byte, &ep, 16);
            if (*ep) {
                //applog(LOG_ERR, "hex2bin failed on '%s'", hex_byte);
                return false;
            }
            p++;
            hexstr += 2;
            len--;
        }

        return(!len) ? true : false;
/*	return (len == 0 && *hexstr == 0) ? true : false; */
    }


    Work::Work(const Json::Value &value, const std::string &coinbase_addr) throw(miner_exception)
    {
        if ( !( value.isMember("height") && value.isMember("version") && value.isMember("previousblockhash") ) )
        {
            throw miner_exception("Invalid work received");
        }

        auto height = value["height"];
        cout << "Height " << height << " " << value["height"].type() << endl;
        auto version = value["version"];
        cout << "Version " << hex << version << dec << " " << value["version"].type() << endl;
        auto previousblockhash = value["previousblockhash"];
        cout << "previousblockhash " << previousblockhash.asString() << " " << value["previousblockhash"].type() << endl;
        // TODO
        //validate_hash(previousblockhash.asString());

        auto curtime = value["curtime"];
        cout << "curtime " << curtime << " " << value["curtime"].type() << endl;
        auto bits = value["bits"];
        cout << "bits " << bits << " " << value["bits"].type() << endl;

        auto transactions = value["transactions"];
        cout << "transactions " << transactions.size() << " " << value["transactions"].type() << endl;
        auto transactions_data_len = 0;
        for ( auto txn : transactions )
        {
            auto data = txn["data"].asString();
            transactions_data_len += data.size() / 2;
            cout << data << endl;
        }



        auto coinbasevalue = value["coinbasevalue"].asInt64();
        cout << "coinbasevalue " << coinbasevalue << " " << value["coinbasevalue"].type() << endl;

        //this->target = value["target"].asInt64();
        //cout << "target " << target << " " << value["target"].type() << endl;


        (void)coinbase_addr;

        // TODO
        //
        // applog(LOG_ERR, "No payout address provided");
        //

        // Part 1
        // 41 bytes = 4 bytes (version) + 1 bytes ( for one coinbase txncount ) + 32 bytes for txout hash (is null for coinbase) + 4 (index)
        bstring8 part1(41, 0); // version, txncount, prevhash, pretxnindex
        set<uint32_t>(part1.data(), 1);
        part1[4] = 1;
        set<uint32_t>(part1.data() + part1.size() - 4, 0xFFFFFFFF);

        // Part 2 -> script length
        bstring8 part2(2, 0);// 2 bytes reserved for size

        // Part 3 -> store height
        bstring8 part3; // script

        // BIP 34: height in coinbase
        for (int n = height.asInt(); n > 0; n >>= 8)
        {
            part3.push_back( static_cast<uint8_t>(n & 0xFF) );
        }

        part2[0] = part3.size() + 1;
        part2[1] = part3.size(); //  Not sure whats the purposer here

        // Part 4 sequence (4 bytes)
        bstring8 part4(4, 0xFF);

        // Part 5 outputs count ( 1 byte)
        bstring8 part5(1, 1); // output count

        // Part 6 coin base value 8 bytes -> 64 bit integer
        bstring8 part6(8, 0); // value of coinbase
        set<uint32_t>(part6.data(), (uint32_t)coinbasevalue);
        set<uint32_t>(part6.data() + 4, (uint32_t)( coinbasevalue >> 32 ));

        // Part 8
        bstring8 part8(25, 0);
        // wallet address for coinbase reward
        auto pk_script_size = address_to_script(part8.data(), part8.size(), "ygZyLqQsPWeczrG4Dm4R3oFKq1Wv9eEMBZ");
        if (!pk_script_size)
        {
            //fprintf(stderr, "invalid address -- '%s'\n", arg);
            throw miner_exception("Invalid coinbase address");
        }

        // Part 7 tx out script length
        bstring8 part7(1, (uint8_t)pk_script_size); // txout script length

        bstring8 part9(4, 0);


        const std::string kCoinbaseSig = "ranjeet miner";
        // TODO coinbaseaux

        for( auto part : std::vector<bstring8 >{std::move(part1)
                , std::move(part2)
                , std::move(part3)
                , std::move(part4)
                , std::move(part5)
                , std::move(part6)
                , std::move(part7)
                , std::move(bstring8(part8.begin(), part8.begin() + pk_script_size))
                , std::move(part9)})
        {
            coinbase_txn.insert(coinbase_txn.end(), part.begin(), part.end());
        }

        //uint8_t* cb_ptr = nullptr;
        bstring8 txc_vi(9, 0);
        auto n = varint_encode(txc_vi.data(), 1 + transactions.size());
        std::unique_ptr<char []> txn_data(new char[2 * (n + coinbase_txn.size() + transactions_data_len) + 1]);
        bin2hex(txn_data.get(), txc_vi.data(), n);
        bin2hex(txn_data.get() + 2 * n, coinbase_txn.data(), coinbase_txn.size());

        //std::vector<uint256> vtxn;
        //uint256 merkle_root = CalcMerkleHash(height, pos);
        /* generate merkle root */
        using MerkleTreeElement = std::array<uint8_t, 32>; // 32 is hash bytes
        std::vector<MerkleTreeElement> merkle_tree( ( 1 + transactions.size() + 1 ) & 0xFFFFFFFF, MerkleTreeElement{});
        sha256d(merkle_tree[0].data(), coinbase_txn.data(), coinbase_txn.size());

        for (int i = 0; i < int(transactions.size()); ++i)
        {
            auto tx_hex = transactions[i]["data"].asString();
            const int tx_size = tx_hex.size() ? (int) (tx_hex.size() / 2) : 0;
            bstring8 tx(tx_size);
            if (!hex2bin(tx.data(), tx_hex.data(), tx_size))
            {
                //applog(LOG_ERR, "JSON invalid transactions");
                throw miner_exception("Invalid hex2 bin conversion");
            }

            sha256d(merkle_tree[1 + i].data(), tx.data(), tx_size);
            // TODO
            // if (!submit_coinbase)
            //auto diff = txn_data.end() - txn_data.begin();
            std::strcat(txn_data.get(), tx_hex.c_str());
        }

        m_txn_data.append(txn_data.get());

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
        bstring32 block_header_part1(1, version.asInt());

        // Part 2 prev hash
        bstring32 block_header_part2(8, 0);
        std::string prevhash = previousblockhash.asString();
        for (int i = 0; i < 8; i++)
        	block_header_part2[7 - i] = le32dec(prevhash.data() + i);

        // Part 3 merkle hash
        bstring32 block_header_part3(8, 0);
        for (int i = 0; i < 8; i++)
        	block_header_part3[i] = be32dec((uint32_t *)merkle_tree[0].data() + i);

        // Part 4 current time
        bstring32 block_header_part4(1, swab32(curtime.asInt()));

        // Part 5 bits
        auto bits_int = std::stoi(bits.asString());
        bstring32 block_header_part5(1, le32dec(&bits_int));

        // Part 6 height -> egihash specific not in Bitcoin
        bstring32 block_header_part6(1, swab32(height.asInt()));

        // Part 7 nonce
        bstring32 block_header_part7(52, 0);
        block_header_part7[1] = 0x80000000;
        block_header_part7[12] = 0x00000280;

        // Target
        bstring32 block_header_part8(8, 0);
        int counter = 0;

        auto target_hex = value["target"].asString();
        bstring32 target_bin(8, 0);
        hex2bin((uint8_t*)target_bin.data(), target_hex.data(), target_hex.size());

        for (int i = 0; i < 8; i++)
        	target_bin[7 - i] = be32dec(target_bin.data() + i);

        this->target = std::move(target_bin);
        this->height = height.asInt();

        for( auto part : std::vector<bstring32>{std::move(block_header_part1)
                , std::move(block_header_part2)
                , std::move(block_header_part3)
                , std::move(block_header_part4)
                , std::move(block_header_part5)
                , std::move(block_header_part6)
                , std::move(block_header_part7)
        		, std::move(block_header_part8)
        })
        {
            data.insert(data.end(), part.begin(), part.end());
        }

        auto size = data.size();
        cout << size << endl;
    }

    Solution::Solution(const Work &work)
    {
        if (work.coinbase_txn.size())
        {
            /* gbt */
            //char data_hex[2 * work.data_bytes_size() + 1];
            for(int i = 0; i < int(work.data.size()); i++)
                be32enc(const_cast<uint32_t*>(work.data.data() + i), work.data[i]);

            ///bin2hex(data_hex, const_cast<uint8_t*>(reinterpret_cast<uint8_t*>(work.data.data())), 84);
            std::string req(128 + 2 * 84 + work.m_txn_data.size(), '\0');

            /*sprintf(req,
                        "{\"method\": \"submitblock\", \"params\": [\"%s%s\"], \"id\":4}\r\n",
                        data_str, work->txs);
            }*/

            //applog(LOG_DEBUG, "DEBUG: submitblock  %s \n", req);
            /*val = json_rpc_call(curl, rpc_url, rpc_userpass, req, NULL, 0);

            free(req);
            if (unlikely(!val)) {
                applog(LOG_ERR, "submit_upstream_work json_rpc_call failed");
                goto out;
            }

            res = json_object_get(val, "result");
            if (json_is_object(res)) {
                char *res_str;
                bool sumres = false;
                void *iter = json_object_iter(res);
                while (iter) {
                    if (json_is_null(json_object_iter_value(iter))) {
                        sumres = true;
                        break;
                    }
                    iter = json_object_iter_next(res, iter);
                }
                res_str = json_dumps(res, 0);
                share_result(sumres, work, res_str);
                free(res_str);
            } else
                share_result(json_is_null(res), work, json_string_value(res));

            json_decref(val);*/

        }
    }


    bool Worker::start()
	{
		cout << "start working" << m_name;
		std::lock_guard<std::mutex> lock(m_mwork);

		if (m_tworker)
		{
			//auto ex = State::Stopped;
			//m_state.compare_exchange_strong(ex, State::Starting);
            m_state = State::Starting;
		}
		else
		{
			m_state = State::Starting;
            m_tworker.reset(new thread([&]()
			{
				//setThreadName(m_name.c_str());
				cout << "Worker Thread begins";
				while (m_state != State::Killing)
				{
					auto ex = State::Starting;
					//bool ok = m_state.compare_exchange_strong(ex, State::Started);
					m_state = State::Started;
                    cout << "Trying to set Started: Thread was" << (unsigned)ex << "; ";// << ok;
					//(void)ok;

					try
					{
						while(1)
							this_thread::sleep_for(chrono::milliseconds(1000));
						//workLoop();
					}
					catch (std::exception const& _e)
					{
						//clog(WarnChannel) << "Exception thrown in Worker thread: " << _e.what();
					}

					//ex = m_state.exchange(State::Stopped);
					cout << "State: Stopped: Thread was" << (unsigned)ex;
					if (ex == State::Killing || ex == State::Starting)
					{
						//m_state.exchange(ex);
					}

					while (m_state == State::Stopped)
					{
						this_thread::sleep_for(chrono::milliseconds(20));
					}
				}
			}));
	//		cnote << "Spawning" << m_name;
		}

		while (m_state == State::Starting)
		{
			this_thread::sleep_for(chrono::microseconds(20));
		}

        return true;
	}

	bool Worker::stop()
	{
		std::lock_guard<std::mutex> lock(m_mwork);
		if (m_tworker)
		{
			//auto ex = State::Started;
			//m_state.compare_exchange_strong(ex, State::Stopping);

			while (m_state != State::Stopped)
			{
				this_thread::sleep_for(chrono::microseconds(20));
			}
		}

        return true;
	}

	Worker::~Worker()
	{
		std::lock_guard<std::mutex> lock(m_mwork);
		if (m_tworker)
		{
			//m_state.exchange(State::Killing);
            m_tworker->join();
            m_tworker.reset();
		}
	}



    CPUMiner::CPUMiner(Plant &plant)
    :Miner("cpu-", plant)
    {
        /*try
        {
            while(true)
            {
                auto const workNow = work();
            }
        }
        catch(...)
        {
            //cwarn << "OpenCL Error:" << _e.what() << _e.err();
        }*/
    }

    void CPUMiner::workLoop()
    {
    	auto work = Work();
    	uint32_t max_nonce;
		uint64_t hashes_done;

    	uint32_t _ALIGN(128) hash[8];
		uint32_t _ALIGN(128) endiandata[21];
		uint32_t *pdata = work.data.data();
		uint32_t *ptarget = work.target.data();

		const uint32_t Htarg = ptarget[7];
		const uint32_t first_nonce = pdata[20];
		uint32_t nonce = first_nonce;
		//volatile uint8_t *restart = &(work_restart[thr_id].restart);

		//if (opt_benchmark)
	//		ptarget[7] = 0x0cff;

		for (int k=0; k < 20; k++)
			be32enc(&endiandata[k], pdata[k]);

		do
		{
			be32enc(&endiandata[20], nonce);
			//x11hash(hash, endiandata);
			auto hash_res = egihash_calc(work.height, nonce, endiandata);
			memcpy(hash, hash_res.b, sizeof(hash_res.b));

			if (hash[7] <= Htarg && fulltest(hash, ptarget)) {
				//work_set_target_ratio(work, hash);
				auto nonceForHash = be32dec(&nonce);
				pdata[20] = nonceForHash;
				hashes_done = nonce - first_nonce;

				Solution sol;
				sol.nonce = nonce;
				sol.hash = hash_res;
				sol.data = work.data;
				setSolution(sol);
				return;
			}
			nonce++;

		} while (nonce < max_nonce);// && !(*restart));

		pdata[20] = be32dec(&nonce);
		hashes_done = nonce - first_nonce + 1;
    }
}
