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

#include <jsonrpccpp/client/connectors/httpclient.h>

#include "egihash/egihash.h"
#include "energiminer/common.h"
#include "energiminer/solution.h"
#include "energiminer/work.h"
#include "energiminer/mineplant.h"


#include <memory>
#include <sstream>
#include <iomanip>
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


#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <boost/optional.hpp>
#include "libegihash-cl/OpenCLMiner.h"

// Win32 GetMessage macro intereferes with jsonrpc::JsonRpcException::GetMessage() member function
#if defined(_WIN32) && defined(GetMessage)
#undef GetMessage
#endif

using namespace std;
using namespace boost::algorithm;
using namespace energi;
using namespace egihash;

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

	bool interpretOption(int& i, int argc, char** argv);

	void execute();

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
			<< "    --cl-local-work Set the OpenCL local work size. Default is " << OpenCLMiner::c_defaultLocalWorkSize << endl
			<< "    --cl-global-work Set the OpenCL global work size as a multiple of the local work size. Default is " << OpenCLMiner::c_defaultGlobalWorkSizeMultiplier << " * " << OpenCLMiner::c_defaultLocalWorkSize << endl
			<< "    --cl-parallel-hash <1 2 ..8> Define how many threads to associate per hash. Default=8" << endl
			;
	}

private:
    void doSimulation(int difficulty = 20);

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
    void doGBT();
    void doStratum();

private:
	/// Operating mode.
	OperationMode mode;

	/// Mining options
	bool should_mine = true;
	MinerExecutionMode m_minerExecutionMode = MinerExecutionMode::kCPU;

	unsigned m_openclPlatform = 0;
	unsigned m_miningThreads = UINT_MAX;
	bool m_shouldListDevices = false;

	unsigned m_openclDeviceCount = 0;
	unsigned m_openclDevices[16];
	unsigned m_openclThreadsPerHash = 8;

	unsigned m_globalWorkSizeMultiplier = energi::OpenCLMiner::c_defaultGlobalWorkSizeMultiplier;
	unsigned m_localWorkSize = energi::OpenCLMiner::c_defaultLocalWorkSize;
	unsigned m_dagLoadMode = 0; // parallel
	unsigned m_dagCreateDevice = 0;


	/// Benchmarking params
	unsigned m_benchmarkWarmup = 15;
	unsigned m_parallelHash    = 4;
	unsigned m_benchmarkTrial = 3;
	unsigned m_benchmarkTrials = 5;
	unsigned m_benchmarkBlock = 0;


	/// Farm params
	bool m_farmRecheckSet = false;
	int m_worktimeout = 180;
	unsigned m_farmRetries = 0;
	unsigned max_retries_ = 20;
    unsigned m_stratumProtocol;
	unsigned m_farmRecheckPeriod = 500;
	unsigned m_defaultStratumFarmRecheckPeriod = 2000;
    std::string m_farmURL = "http://192.168.0.22:9998";
	std::string m_farmFailOverURL = "";
	std::string m_energiURL = m_farmURL;
	std::string m_fport = "";
	std::string coinbase_addr_;
};
