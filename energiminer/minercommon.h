/*
 * minercommon.h
 *
 *  Created on: Nov 22, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_MINERCOMMON_H_
#define ENERGIMINER_MINERCOMMON_H_

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
#include <jsoncpp/json/json.h>
#include "egihash/egihash.h"


#define _ALIGN(x) __attribute__ ((aligned(x)))

static bool fulltest(const uint32_t *hash, const uint32_t *target)
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

std::string GetHex(const uint8_t* data, unsigned int size);



namespace energi
{
    class miner_exception : public std::exception
    {
    public:
        miner_exception(const std::string &reason):m_reason(reason){}
        const char* what() const _GLIBCXX_USE_NOEXCEPT override
        {
            return m_reason.c_str();
        }
    private:
        std::string m_reason;
    };


    // Binary string of uint8_t, just like string is of type char
    using bstring8 = std::vector<uint8_t>;
    template <typename T> void set(const uint8_t* ptr, T value) { *reinterpret_cast<T*>(const_cast<uint8_t*>(ptr)) = value; }

    // 32 byte vector
    using bstring32 = std::vector<uint32_t>;

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
			memcpy(hashPrevBlock, prevHash.c_str(), (std::min)(prevHash.size(), sizeof(hashPrevBlock)));

			auto inputMerkleHashPrevBlock = reinterpret_cast<const uint8_t*>(data) + 36;
			auto merkleRoot = GetHex(inputMerkleHashPrevBlock, 32);
			memcpy(hashMerkleRoot, merkleRoot.c_str(), (std::min)(merkleRoot.size(), sizeof(hashMerkleRoot)));
		}
	};
	#pragma pack(pop)





	struct Work
	{
    	// Valid work
        Work(const Json::Value &value, const std::string &coinbase_addr) throw(miner_exception);
        // No work, probably initialized with assignment
        Work(){}
        Work(const Work&) = default;
        Work(Work &&) = delete;
        Work& operator = (const Work &) = default;
        Work& operator = (Work &&) = delete;

        bool operator==(const Work& another)
        {
            return this->target == another.target && this->height == another.height;
        }

        int             			height = 0;
        bstring32           		target;
        bstring32           		data;
        bstring8            		coinbase_txn;
        std::string         		m_txn_data;
        egihash::h256_t				m_headerHash;

        int data_bytes_size() const
        {
            return data.size() * sizeof(uint32_t);
        }
	};

	// Solution basically is used to submit block
	// Solution is initialized with the work once miner mines a block
	struct Solution
	{
        bstring32           		data;
        std::string     transaction_hex;

        Solution(){}
        std::string getSubmitBlockData() const;
//        Solution& operator = (const Solution&)
//        {
//        }
	};

	// test
	class Worker
	{
		enum class State : unsigned char
		{
			Starting,
			Started,
			Stopping,
			Stopped,
			Killing
		};

	public:
		Worker(const std::string& name)
		: m_name(name)
		{}

		Worker(Worker const&) = default;
		Worker& operator=(Worker const&) = default;
		virtual ~Worker();

		/// Starts a new thread;
		bool start();
		/// Stop thread;
		bool stop();

		bool shouldStop() const
		{
			return m_state != State::Started;
		}

		// Since work is assigned to the worked by the plant and worker is doing the real
		// work in a thread, protect the data carefully.
		virtual void setWork(const Work& work)
		{
			std::lock_guard<std::mutex> lock(m_mutex_work);
			m_work = work;

			onSetWork();
		}

		// Work should be copied with a guard
		void copyWork(Work &dest) const
		{
			std::lock_guard<std::mutex> l(m_mutex_work);
			dest = m_work;
		}

	protected:
		virtual void onSetWork() = 0;
		// run in a thread
		// This function is meant to run some logic in a loop.
		// Should quit once done or when new work is assigned
		virtual void trun(){}
	private:

		std::string 					m_name;
		mutable std::mutex 				m_mutex_work; // protext work since its actually used in a thread
		Work							m_work;

		std::unique_ptr<std::thread> 	m_tworker;
		//std::atomic<State> 			m_state { State::Starting };
        State 				            m_state { State::Starting };
	};


    class MinePlant;
	class Miner: public Worker
	{
	public:
		Miner(std::string const& name, MinePlant &plant):
			Worker(name + std::to_string(0)),
            m_plant(plant)
		{}

		Miner(Miner && m) = default;

		virtual ~Miner() = default;

		void setWork(Work const& work)
		{
			Worker::setWork(work);
		}

	protected:
        MinePlant &m_plant;
	};


	class MinePlant
	{
	public:
		using CBSolutionFound = std::function<bool(const Solution&)>;
		using VMiners = std::vector<std::shared_ptr<Miner>>;

		~MinePlant()
		{
			stop();
		}

		bool start(const VMiners &vMiner
					, const CBSolutionFound& cb_solution_found)
		{
			//std::lock_guard<std::mutex> lock(m_mutex_miner);

			m_cb_solution_found = cb_solution_found;

			if ( m_started )
            {
                return m_started;
            }

			m_started = true;

            for ( auto &miner : vMiner)
            {
                m_miners.push_back(miner);
                m_started &= m_miners.back()->start();
            }

            // Failure ? return right away
            return m_started;
		}

		void stop()
		{
//			std::lock_guard<std::mutex> lock(m_mutex_miner);
			for ( auto &miner : m_miners)
			{
				miner->stop();
			}
		}

		void setWork(const Work& work)
		{
			//Collect hashrate before miner reset their work
			//collectHashRate();

			// Set work to each miner
			//std::lock_guard<std::mutex> lock(m_mutex_miner);
/*
			if (_wp.header == m_work.header && _wp.startNonce == m_work.startNonce)
			{
				return;
			}
*/

			m_work = work;
			for (auto &miner: m_miners)
			{
				miner->setWork(m_work);
			}
		}

		void restart()
		{
			stop();
		}


		void submit(const Solution &sol)
		{
			m_cb_solution_found(sol);
		}

		bool m_started = false;
        Solution m_solution;
        CBSolutionFound m_cb_solution_found;
	private:
        VMiners m_miners;
		Work m_work;

		//std::atomic<bool> m_isMining = {false};

		//mutable std::mutex x_progress;
		//mutable WorkingProgress m_progress;

		//SolutionFound m_onSolutionFound;
		//MinerRestart m_onMinerRestart;
        //bool m_has_found_block = false;
	};


    class CPUMiner: public Miner
    {
    public:
        CPUMiner(MinePlant& _farm);
        ~CPUMiner()
        {}
    protected:
        void onSetWork() override;
        void trun() override;
    };

//    class CLMiner: public Miner
//    {
//    public:
//        CLMiner(MinePlant& _farm);
//        ~CLMiner();
//    protected:
//        void onSetWork() override;
//        void trun() override;
//    };
}

#endif /* ENERGIMINER_MINERCOMMON_H_ */
