#pragma once

/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file MinerAux.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 * CLI module for mining.
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
#include <condition_variable>

#include "minercommon.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <boost/optional.hpp>

#include <libethcore/Exceptions.h>
#include <libdevcore/SHA3.h>
#include <libethcore/EthashAux.h>
#include <libethcore/Farm.h>
#include <libethash-cl/CLMiner.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include "minercommon.h"
#include "FarmClient.h"
#include "egihash/egihash.h"


#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <memory>
#include <sstream>
#include <iomanip>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>


using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace boost::algorithm;


/*
using bstring8 = basic_string<uint8_t>;
using bstring32 = basic_string<uint32_t>;
*/



/*	int scanhash_egihash(int thr_id, struct work *work, uint32_t max_nonce, uint64_t *hashes_done)
{
	uint32_t _ALIGN(128) hash[8];
	uint32_t _ALIGN(128) endiandata[21];
	uint32_t *pdata = work->data;
	uint32_t *ptarget = work->target;

	const uint32_t Htarg = ptarget[7];
	const uint32_t first_nonce = pdata[20];
	uint32_t nonce = first_nonce;
	volatile uint8_t *restart = &(work_restart[thr_id].restart);

	if (opt_benchmark)
		ptarget[7] = 0x0cff;

	for (int k=0; k < 20; k++)
		be32enc(&endiandata[k], pdata[k]);

	do {
		be32enc(&endiandata[20], nonce);
		//x11hash(hash, endiandata);
		egihash_calc(hash, work->height, nonce, endiandata);

		if (hash[7] <= Htarg && fulltest(hash, ptarget)) {
			work_set_target_ratio(work, hash);
			auto nonceForHash = be32dec(&nonce);
			pdata[20] = nonceForHash;
			*hashes_done = nonce - first_nonce;
			return 1;
		}
		nonce++;

	} while (nonce < max_nonce && !(*restart));

	pdata[20] = be32dec(&nonce);
	*hashes_done = nonce - first_nonce + 1;
	return 0;
}*/




class BadArgument: public Exception
{
};

struct MiningChannel: public LogChannel
{
	static const char* name() { return EthGreen "  m"; }
	static const int verbosity = 2;
	static const bool debug = false;
};

#define minelog clog(MiningChannel)

inline std::string toJS(unsigned long _n)
{
	std::string h = toHex(toCompactBigEndian(_n, 1));
	// remove first 0, if it is necessary;
	std::string res = h[0] != '0' ? h : h.substr(1);
	return "0x" + res;
}

class MinerCLI
{
public:
	enum class OperationMode
	{
		None,
		Benchmark,
		Simulation,
		GBT,
		Stratum
	};

	MinerCLI(OperationMode _mode = OperationMode::Simulation): mode(_mode) {}

	bool interpretOption(int& i, int argc, char** argv)
	{
		string arg = argv[i];
		if ((arg == "--gbt") && i + 1 < argc)
		{
			mode = OperationMode::GBT;
			m_farmURL = argv[++i];
			m_activeFarmURL = m_farmURL;
		}
		else if ((arg == "--coinbase-addr") && i + 1 < argc)
		{
			m_coinbaseAddr = argv[++i];
		}
		else if (arg == "--farm-recheck" && i + 1 < argc)
			try {
				m_farmRecheckSet = true;
				m_farmRecheckPeriod = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				BOOST_THROW_EXCEPTION(BadArgument());
			}
		else if (arg == "--farm-retries" && i + 1 < argc)
			try {
				m_maxFarmRetries = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				BOOST_THROW_EXCEPTION(BadArgument());
			}
		else if (arg == "--opencl-platform" && i + 1 < argc)
			try {
				m_openclPlatform = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				BOOST_THROW_EXCEPTION(BadArgument());
			}
		else if (arg == "--opencl-devices" || arg == "--opencl-device")
			while (m_openclDeviceCount < 16 && i + 1 < argc)
			{
				try
				{
					m_openclDevices[m_openclDeviceCount] = stol(argv[++i]);
					++m_openclDeviceCount;
				}
				catch (...)
				{
					i--;
					break;
				}
			}
		else if(arg == "--cl-parallel-hash" && i + 1 < argc) {
			try {
				m_openclThreadsPerHash = stol(argv[++i]);
				if(m_openclThreadsPerHash != 1 && m_openclThreadsPerHash != 2 &&
				   m_openclThreadsPerHash != 4 && m_openclThreadsPerHash != 8) {
					BOOST_THROW_EXCEPTION(BadArgument());
				} 
			}
			catch(...) {
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				BOOST_THROW_EXCEPTION(BadArgument());
			}
		}
		else if ((arg == "--cl-global-work" || arg == "--cuda-grid-size")  && i + 1 < argc)
			try {
				m_globalWorkSizeMultiplier = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				BOOST_THROW_EXCEPTION(BadArgument());
			}
		else if ((arg == "--cl-local-work" || arg == "--cuda-block-size") && i + 1 < argc)
			try {
				m_localWorkSize = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				BOOST_THROW_EXCEPTION(BadArgument());
			}
		else if (arg == "--list-devices")
			m_shouldListDevices = true;
		else if ((arg == "-L" || arg == "--dag-load-mode") && i + 1 < argc)
		{
			/*string mode = argv[++i];
			if (mode == "parallel") m_dagLoadMode = DAG_LOAD_MODE_PARALLEL;
			else if (mode == "sequential") m_dagLoadMode = DAG_LOAD_MODE_SEQUENTIAL;
			else if (mode == "single")
			{
				m_dagLoadMode = DAG_LOAD_MODE_SINGLE;
				m_dagCreateDevice = stol(argv[++i]);
			}
			else
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				BOOST_THROW_EXCEPTION(BadArgument());
			}*/
		}
		else if (arg == "--benchmark-warmup" && i + 1 < argc)
			try {
				m_benchmarkWarmup = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				BOOST_THROW_EXCEPTION(BadArgument());
			}
		else if (arg == "--benchmark-trial" && i + 1 < argc)
			try {
				m_benchmarkTrial = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				BOOST_THROW_EXCEPTION(BadArgument());
			}
		else if (arg == "--benchmark-trials" && i + 1 < argc)
			try
			{
				m_benchmarkTrials = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				BOOST_THROW_EXCEPTION(BadArgument());
			}
		/*else if (arg == "-G" || arg == "--opencl")
			m_minerType = MinerType::CL;
		else if (arg == "-X" || arg == "--cuda-opencl")
		{
			m_minerType = MinerType::Mixed;
		}*/
		else if (arg == "-M" || arg == "--benchmark")
		{
			mode = OperationMode::Benchmark;
			if (i + 1 < argc)
			{
				string m = boost::to_lower_copy(string(argv[++i]));
				try
				{
					m_benchmarkBlock = stol(m);
				}
				catch (...)
				{
					if (argv[i][0] == 45) { // check next arg
						i--;
					}
					else {
						cerr << "Bad " << arg << " option: " << argv[i] << endl;
						BOOST_THROW_EXCEPTION(BadArgument());
					}
				}
			}
		}
		else if (arg == "-Z" || arg == "--simulation") {
			mode = OperationMode::Simulation;
			if (i + 1 < argc)
			{
				string m = boost::to_lower_copy(string(argv[++i]));
				try
				{
					m_benchmarkBlock = stol(m);
				}
				catch (...)
				{
					if (argv[i][0] == 45) { // check next arg
						i--;
					}
					else {
						cerr << "Bad " << arg << " option: " << argv[i] << endl;
						BOOST_THROW_EXCEPTION(BadArgument());
					}
				}
			}
		}
		else if ((arg == "-t" || arg == "--mining-threads") && i + 1 < argc)
		{
			try
			{
				m_miningThreads = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				BOOST_THROW_EXCEPTION(BadArgument());
			}
		}
		else
			return false;
		return true;
	}

	void execute()
	{
		/*if (m_shouldListDevices)
		{
			if (m_minerType == MinerType::CL || m_minerType == MinerType::Mixed)
				CLMiner::listDevices();
			exit(0);
		}

		if (m_minerType == MinerType::CL || m_minerType == MinerType::Mixed)
		{
			if (m_openclDeviceCount > 0)
			{
				CLMiner::setDevices(m_openclDevices, m_openclDeviceCount);
				m_miningThreads = m_openclDeviceCount;
			}
			
			CLMiner::setThreadsPerHash(m_openclThreadsPerHash);
			if (!CLMiner::configureGPU(
					m_localWorkSize,
					m_globalWorkSizeMultiplier,
					m_openclPlatform,
					0,
					m_dagLoadMode,
					m_dagCreateDevice
				))
				exit(1);
			CLMiner::setNumInstances(m_miningThreads);
		}*/
//		if (mode == OperationMode::Benchmark)
//			doBenchmark(m_minerType, m_benchmarkWarmup, m_benchmarkTrial, m_benchmarkTrials);
		if (mode == OperationMode::GBT)
			doGBT2(m_activeFarmURL, m_farmRecheckPeriod);
		else if (mode == OperationMode::Simulation) {

        }
        //doSimulation(m_minerType);
	}

	static void streamHelp(ostream& _out)
	{
		_out
			<< "Work farming mode:" << endl
			<< "    --gbt <url>  Put into mining farm mode with the work server at URL (default: http://u:p@127.0.0.1:8545)" << endl
			<< "    --coinbase-addr ADDRESS  Payout address for solo mining" << endl
			<< "Benchmarking mode:" << endl
			<< "    -M [<n>],--benchmark [<n>] Benchmark for mining and exit; Optionally specify block number to benchmark against specific DAG." << endl
			<< "    --benchmark-warmup <seconds>  Set the duration of warmup for the benchmark tests (default: 3)." << endl
			<< "    --benchmark-trial <seconds>  Set the duration for each trial for the benchmark tests (default: 3)." << endl
			<< "    --benchmark-trials <n>  Set the number of benchmark trials to run (default: 5)." << endl
			<< "Simulation mode:" << endl
			<< "    -Z [<n>],--simulation [<n>] Mining test mode. Used to validate kernel optimizations. Optionally specify block number." << endl
			<< "Mining configuration:" << endl
			<< "    --opencl-platform <n>  When mining using -G/--opencl use OpenCL platform n (default: 0)." << endl
			<< "    --opencl-device <n>  When mining using -G/--opencl use OpenCL device n (default: 0)." << endl
			<< "    --opencl-devices <0 1 ..n> Select which OpenCL devices to mine on. Default is to use all" << endl
			<< "    -t, --mining-threads <n> Limit number of CPU/GPU miners to n (default: use everything available on selected platform)" << endl
			<< "    --list-devices List the detected OpenCL/CUDA devices and exit. Should be combined with -G or -U flag" << endl
			<< "    -L, --dag-load-mode <mode> DAG generation mode." << endl
			<< "        parallel    - load DAG on all GPUs at the same time (default)" << endl
			<< "        sequential  - load DAG on GPUs one after another. Use this when the miner crashes during DAG generation" << endl
			<< "        single <n>  - generate DAG on device n, then copy to other devices" << endl
			<< "    --cl-local-work Set the OpenCL local work size. Default is " << CLMiner::c_defaultLocalWorkSize << endl
			<< "    --cl-global-work Set the OpenCL global work size as a multiple of the local work size. Default is " << CLMiner::c_defaultGlobalWorkSizeMultiplier << " * " << CLMiner::c_defaultLocalWorkSize << endl
			<< "    --cl-parallel-hash <1 2 ..8> Define how many threads to associate per hash. Default=8" << endl
			;
	}

private:
/*
	void doBenchmark(MinerType _m, unsigned _warmupDuration = 15, unsigned _trialDuration = 3, unsigned _trials = 5)
	{
		BlockHeader genesis;
		genesis.setNumber(m_benchmarkBlock);
		genesis.setDifficulty(1 << 18);
		cdebug << genesis.boundary();

		Farm f;
		map<string, Farm::SealerDescriptor> sealers;
		sealers["opencl"] = Farm::SealerDescriptor{&CLMiner::instances, [](FarmFace& _farm, unsigned _index){ return new CLMiner(_farm, _index); }};
		f.setSealers(sealers);
		f.onSolutionFound([&](Solution) { return false; });

		string platformInfo = _m == MinerType::CL ? "CL" : "CUDA";
		cout << "Benchmarking on platform: " << platformInfo << endl;

		cout << "Preparing DAG for block #" << m_benchmarkBlock << endl;
		//genesis.prep();

		genesis.setDifficulty(u256(1) << 63);
		if (_m == MinerType::CL)
			f.start("opencl", false);
		f.setWork(WorkPackage{genesis});

		map<uint64_t, WorkingProgress> results;
		uint64_t mean = 0;
		uint64_t innerMean = 0;
		for (unsigned i = 0; i <= _trials; ++i)
		{
			if (!i)
				cout << "Warming up..." << endl;
			else
				cout << "Trial " << i << "... " << flush;
			this_thread::sleep_for(chrono::seconds(i ? _trialDuration : _warmupDuration));

			auto mp = f.miningProgress();
			if (!i)
				continue;
			auto rate = mp.rate();

			cout << rate << endl;
			results[rate] = mp;
			mean += rate;
		}
		f.stop();
		int j = -1;
		for (auto const& r: results)
			if (++j > 0 && j < (int)_trials - 1)
				innerMean += r.second.rate();
		innerMean /= (_trials - 2);
		cout << "min/mean/max: " << results.begin()->second.rate() << "/" << (mean / _trials) << "/" << results.rbegin()->second.rate() << " H/s" << endl;
		cout << "inner mean: " << innerMean << " H/s" << endl;

		exit(0);
	}



	void doSimulation(MinerType _m, int difficulty = 20)
	{
*//*
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        genesis = CreateGenesisBlock(1506586761UL, 0, 0x207fffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x440cbbe939adba25e9e41b976d3daf8fb46b5f6ac0967b0a9ed06a749e7cf1e2"));
        assert(genesis.hashMerkleRoot == uint256S("0x6a4855e61ae0da5001564cee6ba8dcd7bc361e9bb12e76b62993390d6db25bca"));
*//*
		(void)_m;

		BlockHeader genesis;
		genesis.setNumber(m_benchmarkBlock);
		genesis.setDifficulty(0x207fffff);
		cdebug << "BOUNDARY__" << genesis.boundary();

		Farm f;
		map<string, Farm::SealerDescriptor> sealers;
		sealers["opencl"] = Farm::SealerDescriptor{ &CLMiner::instances, [](FarmFace& _farm, unsigned _index){ return new CLMiner(_farm, _index); } };
		f.setSealers(sealers);

		string platformInfo = "CL";
		cout << "Running mining simulation on platform: " << platformInfo << endl;

		cout << "Preparing DAG for block #" << m_benchmarkBlock << endl;
		//genesis.prep();

		genesis.setDifficulty(0x207fffff);

		f.start("opencl", false);

		int time = 0;

		WorkPackage current = WorkPackage(genesis);
		f.setWork(current);
		while (true) {
			bool completed = false;
			Solution solution;
			f.onSolutionFound([&](Solution sol)
			{
				solution = sol;
				return completed = true;
			});
			for (unsigned i = 0; !completed; ++i)
			{
				auto mp = f.miningProgress();

				cnote << "Mining on difficulty " << difficulty << " " << mp;
				this_thread::sleep_for(chrono::milliseconds(1000));
				time++;
			}
			cnote << "Difficulty:" << difficulty << "  Nonce:" << solution.nonce;
			if (EthashAux::eval(current.seed, current.header, solution.nonce).value < current.boundary)
			{
				cnote << "SUCCESS: GPU gave correct result!";
			}
			else
				cwarn << "FAILURE: GPU gave incorrect result!";

			if (time < 12)
				difficulty++;
			else if (time > 18)
				difficulty--;
			time = 0;
			genesis.setDifficulty(u256(1) << difficulty);
			genesis.noteDirty();

			current.header = h256::random();
			current.boundary = genesis.boundary();
			minelog << "Generated random work package:";
			minelog << "  Header-hash:" << current.header.hex();
			minelog << "  Seedhash:" << current.seed.hex();
			minelog << "  Target: " << h256(current.boundary).hex();
			f.setWork(current);

		}
	}*/
/*

	bool validate_hash(const std::string &previousblockhash)
	{
		// TODO
		char hex_byte[3];
		char *ep;

		hex_byte[2] = '\0';

		while (*hexstr && len) {
			if (!hexstr[1]) {
				applog(LOG_ERR, "hex2bin str truncated");
				return false;
			}
			hex_byte[0] = hexstr[0];
			hex_byte[1] = hexstr[1];
			*p = (unsigned char) strtol(hex_byte, &ep, 16);
			if (*ep) {
				applog(LOG_ERR, "hex2bin failed on '%s'", hex_byte);
				return false;
			}
			p++;
			hexstr += 2;
			len--;
		}

		return(!len) ? true : false;
		return (len == 0 && *hexstr == 0) ? true : false;

		return true;
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



    * calculate the hash of a node in the merkle tree (at leaf level: the txid's themselves)
	uint256 CalcMerkleHash(int height, unsigned int pos, const std::vector<uint256> &vTxid) {
	    if (height == 0) {
	        // hash at height 0 is the txids themself
	        return vTxid[pos];
	    } else {
	        // calculate left hash
	        uint256 left = CalcHash(height-1, pos*2, vTxid), right;
	        // calculate right hash if not beyond the end of the array - copy left hash otherwise1
	        if (pos*2+1 < CalcTreeWidth(height-1))
	            right = CalcHash(height-1, pos*2+1, vTxid);
	        else
	            right = left;
	        // combine subhashes
	        return Hash(BEGIN(left), END(left), BEGIN(right), END(right));
	    }
	}



	bool parse(Json::Value &work, Json::Value &mining_info)
	{
		if ( !( value.isMember("height") && value.isMember("version") && value.isMember("previousblockhash") ) )
		{
			return false;
		}

		auto height = value["height"];
		cout << "Height " << height << " " << value["height"].type() << endl;
		auto version = value["version"];
		cout << "Version " << hex << version << dec << " " << value["version"].type() << endl;
		auto previousblockhash = value["previousblockhash"];
		cout << "previousblockhash " << previousblockhash.asString() << " " << value["previousblockhash"].type() << endl;
		validate_hash(previousblockhash.asString());

		auto curtime = value["curtime"];
		cout << "curtime " << curtime << " " << value["curtime"].type() << endl;
		auto bits = value["bits"];
		cout << "bits " << bits << " " << value["bits"].type() << endl;

		auto transactions = value["transactions"];
		cout << "transactions " << transactions.size() << " " << value["transactions"].type() << endl;
		for ( auto txn : transactions )
		{
			auto data = txn["data"].asString();
			cout << data << endl;
		}


		auto pk_script_size = 1;//address_to_script(pk_script, sizeof(pk_script), arg);
		if (!pk_script_size)
		{
			//fprintf(stderr, "invalid address -- '%s'\n", arg);
			return false;
		}

		auto coinbasevalue = value["coinbasevalue"].asInt64();
		cout << "coinbasevalue " << coinbasevalue << " " << value["coinbasevalue"].type() << endl;

		auto target = value["target"].asInt64();
		//cout << "target " << target << " " << value["target"].type() << endl;



		// TODO
		//
		// applog(LOG_ERR, "No payout address provided");
		//

		using bstring8 = basic_string<uint8_t>;
		bstring8 part1(41, 0); // version, txncount, prevhash, pretxnindex
		*reinterpret_cast<uint32_t*>(part1.data()) = 1;
		*reinterpret_cast<uint32_t*>(part1.data() + 4) = 1;
		*reinterpret_cast<uint32_t*>(part1.data() + part1.size() - 4) = 0xFFFFFFFF;

		bstring8 part2(2, 0);// 2 bytes for size
		bstring8 part3; // script

		 BIP 34: height in coinbase
		for (int n = height; n > 0; n >>= 8)
		{
			part3.push_back( static_cast<uint8_t>(n & 0xFF) );
		}

		part2[0] = part3.size() + 1;
		part2[1] = part3.size();

		bstring8 part4(4, 0xFF);  sequence
		bstring8 part5(1, 1); // output count
		bstring8 part6(8, 0); // value of coinbase

		*reinterpret_cast<uint32_t*>(part6.data()) 		= (uint32_t)coinbasevalue;
		*reinterpret_cast<uint32_t*>(part6.data() + 4) 	= (uint32_t)( coinbasevalue >> 32);

		bstring8 part7(1, (uint8_t) pk_script_size); // txout script length
		bstring8 part8(pk_script, part7[0]);
		bstring8 part9(4, 0);



		auto coinbase_txn_size = 100;
		uint8_t* cb_ptr = nullptr;
		bstring8 txc_vi(9, 0);
		auto n = varint_encode(txc_vi.data(), 1 + transactions.size());
		bstring8 txn_data(2 * (n + coinbase_txn_size + transactions.size()) + 1);
		bin2hex(txn_data.data(), txc_vi, n);
		bin2hex(txn_data.data() + 2 * n, cb_ptr, coinbase_txn_size);

		//std::vector<uint256> vtxn;
	    //uint256 merkle_root = CalcMerkleHash(height, pos);

		 assemble block header
	    bstring32 block_header_part1(1, version);
	    bstring32 block_header_part2(8, 0);
	    auto counter = 0;
	    for (auto iter = block_header_part2.rbegin(); iter != block_header_part2.rend(); ++iter, ++counter)
	    	*iter = previousblockhash[counter];

	    bstring32 block_header_part3(8, 0);
	    counter = 0;
	    for (auto iter = block_header_part3.rbegin(); iter != block_header_part3.rend(); ++iter, ++counter)
	    	*iter = previousblockhash[counter];//merkle_root[counter];

	    bstring32 block_header_part4(1, curtime);
	    bstring32 block_header_part5(1, bits.asInt());
	    bstring32 block_header_part6(1, height.asInt());
	    bstring32 block_header_part7(52, 0);
	    block_header_part7[1] = 0x80000000;
	    block_header_part7[12] = 0x00000280;


		if (unlikely(!jobj_binary(val, "target", target, sizeof(target)))) {
			applog(LOG_ERR, "JSON invalid target");
			goto out;
		}

	    bstring32 block_header_part8(8, 0);
	    counter = 0;

	    for (auto iter = block_header_part8.rbegin(); iter != block_header_part8.rend(); ++iter, ++counter)
	      *iter = target[counter];

	}*/

	void doGBT(string & /*_remote*/, unsigned /*_recheckPeriod*/)
	{

	}


	void doGBT2(string & _remote, unsigned _recheckPeriod)
	{
		//(void)_m;
		(void)_remote;
		(void)_recheckPeriod;

		jsonrpc::HttpClient client(m_farmURL);
		GBTClient rpc(client);
        energi::Plant plant;
        std::vector<energi::Miner> vMiners = { energi::CPUMiner(plant) };//, energi::CLMiner(plant) };
        plant.start(vMiners); // start plant full of miners

        // Coinbase address for payment
        std::string coinbase_addr = m_coinbaseAddr;
        energi::Work currentWork;
        std::mutex x_current;
        while (is_mining)
		{
			try
			{
                auto workProgress = energi::WorkProgress::Started;
                energi::Solution solution;
                while(workProgress != energi::WorkProgress::Done)
                {
                    auto gbt = rpc.getBlockTemplate();
                    energi::Work newWork(gbt, coinbase_addr);

                    if ( newWork == currentWork )
                    {
                        this_thread::sleep_for(chrono::milliseconds(5000));
                        workProgress = plant.hasFoundBlock() ? energi::WorkProgress::Done : workProgress;
                        solution = plant.getSolution();
                        continue;
                    }

                    // 1. Got new work
                    // 2. Abandon current work and take new work
                    // 3. miner starts mining for new work
                    currentWork = newWork;
                    plant.setWork(newWork);

                    //auto mining_info = rpc.getMiningInfo();
                    //parse(work, mining_info);
                    //mine();
                    this_thread::sleep_for(chrono::milliseconds(5000));
                    workProgress = plant.hasFoundBlock() ? energi::WorkProgress::Done : workProgress;
                    solution = plant.getSolution();
                }

                // Since solution was found, submit now
                rpc.submitWork(solution);
				break;
			}
			catch (jsonrpc::JsonRpcException& je)
			{
				if (m_maxFarmRetries > 0)
				{
					for (auto i = 3; --i; this_thread::sleep_for(chrono::seconds(1)))
						cerr << je.GetMessage() << endl << je.what() << endl << "JSON-RPC problem. Probably couldn't connect. Retrying in " << i << "... \r";
					cerr << endl;
				}
				else
				{
					cerr << "JSON-RPC problem. Probably couldn't connect." << endl;
				}
			}
		}

		cout << "Hello world: " << m_farmURL << " " << endl;
		//GBTClient *prpc = &rpc;
		exit(0);
	}


	/// Operating mode.
	OperationMode mode;

	/// Mining options
	bool is_mining = true;
	MinerType m_minerType = MinerType::Mixed;
	unsigned m_openclPlatform = 0;
	unsigned m_miningThreads = UINT_MAX;
	bool m_shouldListDevices = false;
	unsigned m_openclDeviceCount = 0;
	unsigned m_openclDevices[16];
	unsigned m_openclThreadsPerHash = 8;
	unsigned m_globalWorkSizeMultiplier = CLMiner::c_defaultGlobalWorkSizeMultiplier;
	unsigned m_localWorkSize = CLMiner::c_defaultLocalWorkSize;
	unsigned m_dagLoadMode = 0; // parallel
	unsigned m_dagCreateDevice = 0;
	/// Benchmarking params
	unsigned m_benchmarkWarmup = 15;
	unsigned m_parallelHash    = 4;
	unsigned m_benchmarkTrial = 3;
	unsigned m_benchmarkTrials = 5;
	unsigned m_benchmarkBlock = 0;
	/// Farm params
	string m_farmURL = "http://192.168.0.22:9998";
	string m_farmFailOverURL = "";
	string m_activeFarmURL = m_farmURL;
	unsigned m_farmRetries = 0;
	unsigned m_maxFarmRetries = 3;
	unsigned m_farmRecheckPeriod = 500;
	unsigned m_defaultStratumFarmRecheckPeriod = 2000;
	bool m_farmRecheckSet = false;
	int m_worktimeout = 180;
	string m_fport = "";
	string m_coinbaseAddr;
};
