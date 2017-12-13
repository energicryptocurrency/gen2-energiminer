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
//using namespace dev;
//using namespace dev::eth;
using namespace boost::algorithm;
using namespace energi;
using namespace egihash;

/*
using bstring8 = basic_string<uint8_t>;
using bstring32 = basic_string<uint32_t>;
*/

enum class MinerType
{
	Mixed,
	CL,
	CUDA
};


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


/// Base class for all exceptions.
struct Exception: virtual std::exception, virtual boost::exception
{
	Exception(std::string _message = std::string()): m_message(std::move(_message)) {}
	const char* what() const noexcept override { return m_message.empty() ? std::exception::what() : m_message.c_str(); }

private:
	std::string m_message;
};

class BadArgument: public Exception
{
};
struct LogChannel { static const char* name(); static const int verbosity = 1; static const bool debug = true; };

struct MiningChannel: public LogChannel
{
	static const char* name() { return EthGreen "  m"; }
	static const int verbosity = 2;
	static const bool debug = false;
};

//#define minelog clog(MiningChannel)

//inline std::string toJS(unsigned long _n)
//{
//	std::string h = toHex(toCompactBigEndian(_n, 1));
//	// remove first 0, if it is necessary;
//	std::string res = h[0] != '0' ? h : h.substr(1);
//	return "0x" + res;
//}

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
			m_coinbase_addr = argv[++i];
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
				m_max_retries = stol(argv[++i]);
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
			string mode = argv[++i];
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
			}
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
		if (m_shouldListDevices)
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
			//CLMiner::setNumInstances(m_miningThreads);
		}

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

	void doGBT(string & /*_remote*/, unsigned /*_recheckPeriod*/)
	{
	}


	/*
	 doGBT function starts Plant and in farm it starts miners intended to mine e.g. CPUMiner And/or Gpuminer
	 doGBT runs a loop where,
	 	 1. it calls getblocktemplate to get the new work
	 	 2. Prepares a Work object, which essentially contains data to be mined after parsing getblocktemplate.
	     3. Since there is no queue concept here because of no need of mining old set of transactions to create new block
	     	Simply reset the Work everytime we get new data
	     	So doGBT sets work on Plant which internally is meant to forward to all Miners.
	     4. One way is to let Plant make a call back as soon as data is ready by using proper synchronization techniques e.g. events
	     5. Other is to make a good guess of a few milliseconds wait to check for next getblocktemplate if is different

	     What goes inside plant
	     6. Inside Plant
	     	a. Plant runs the miners which is also called as a worker, and a worker spawns a thread to do its job and listen for new work
	     	b. As soon as plant gets new Work, it sets the data for all miners and expects miners to restart ASAP on new data.
	     	   Basically by requesting them to finish ASAP old block being mined as there is no point.
	     	   Miners drop existing work and take new Work and start mining fresh.
	     	   As soon as a miner finds a solution, it calls plant onSolutionFind and plant expects it to be picked up on next check in doGBT.

	 	 7. Threading design
	 	 8. doGBT runs in main thread
	 	 	- Plant starts miners (miner is a worker)
			- miner instead spawns a child thread to do the real mine work and also keep ears open to listen for new data.
			- we need to synchronize plant and miner communication

	 */




	void doGBT2(string & _remote, unsigned _recheckPeriod)
	{


        extern bool InitEgiHashDag();

        //InitEgiHashDag();

		//(void)_m;
		(void)_remote;
		(void)_recheckPeriod;

		jsonrpc::HttpClient client(m_farmURL);
		GBTClient rpc(client);

		// Create plant
        energi::MinePlant plant;
        std::mutex mutex_solution;

        // Start plant now with given miners
        // start plant full of miners
        bool m_solution_found = false;
        energi::Solution solution;
        // Note, this is mostly called from a miner thread, but since solution is consumed in main thread after set
        // its safe to not lock the access
        energi::MinePlant::CBSolutionFound cb_solution_finder = [&m_solution_found, &mutex_solution, &solution](const energi::Solution& solution_)->bool {
        	m_solution_found = true;
        	std::lock_guard<std::mutex> l(mutex_solution);
        	solution = solution_;

        	return true;
        };

        // Check started or not
        energi::MinePlant::VMiners vminers;
        //vminers.push_back(std::shared_ptr<energi::Miner>(new energi::CPUMiner(plant)));
        vminers.push_back(std::shared_ptr<energi::Miner>(new energi::CLMiner(plant)));
        if ( !plant.start( vminers, cb_solution_finder) )
        {
        	// TODO add comment
        	return;
        }


        // Coinbase address for payment
        energi::Work current_work;




        // Mine till you can, or retries fail after a limit
        while (m_is_mining)
		{
			try
			{
                m_solution_found = false;
                // Keep checking for new work and mine
                while(!m_solution_found)
                {
                    auto gbt = rpc.getBlockTemplate();
                    energi::Work new_work(gbt, m_coinbase_addr);

                    // check if current work is no different, then skip
                    if ( new_work == current_work )
                    {
                        this_thread::sleep_for(chrono::milliseconds(500));
                        continue;
                    }

                    // 1. Got new work
                    // 2. Abandon current work and take new work
                    // 3. miner starts mining for new work
                    current_work = new_work;
                    plant.setWork(new_work);

                    // 4. Work has been assigned to the plant
                    // 5. Wait and continue for new work
                    // 6. TODO decide on time to wait for
                    this_thread::sleep_for(chrono::milliseconds(100));
                }

                // 7. Since solution was found, submit now
                std::lock_guard<std::mutex> l(mutex_solution);
                rpc.submitSolution(solution);
				break;
			}
			catch (jsonrpc::JsonRpcException& je)
			{
				if (m_max_retries > 100)
				{
					cerr << "JSON-RPC problem. Couldn't connect, will exit now" << endl;
					m_is_mining = false;
				}
				else if (m_max_retries > 0)
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
	bool m_is_mining = true;
	MinerType m_minerType = MinerType::Mixed;
	unsigned m_openclPlatform = 0;
	unsigned m_miningThreads = UINT_MAX;
	bool m_shouldListDevices = false;
	unsigned m_openclDeviceCount = 0;
	unsigned m_openclDevices[16];
	unsigned m_openclThreadsPerHash = 8;
	unsigned m_globalWorkSizeMultiplier = energi::CLMiner::c_defaultGlobalWorkSizeMultiplier;
	unsigned m_localWorkSize = energi::CLMiner::c_defaultLocalWorkSize;
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
	unsigned m_max_retries = 3;
	unsigned m_farmRecheckPeriod = 500;
	unsigned m_defaultStratumFarmRecheckPeriod = 2000;
	bool m_farmRecheckSet = false;
	int m_worktimeout = 180;
	string m_fport = "";
	string m_coinbase_addr;
};
