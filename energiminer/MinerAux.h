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

#include "nrghash/nrghash.h"
#include "common/common.h"
#include "primitives/solution.h"
#include "primitives/work.h"
#include "nrgcore/mineplant.h"


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

#ifdef ETH_ETHASHCL
#include "libegihash-cl/OpenCLMiner.h"
#endif

#ifdef ETH_ETHASHCUDA
#include "libnrghash-cuda/CUDAMiner.h"
#endif

// Win32 GetMessage macro intereferes with jsonrpc::JsonRpcException::GetMessage() member function
#if defined(_WIN32) && defined(GetMessage)
#undef GetMessage
#endif

using namespace std;
using namespace boost::algorithm;
using namespace energi;
using namespace nrghash;

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
			<< "	-S, --stratum <host:port>  Put into stratum mode with the stratum server at host:port" << endl
			<< "    -O, --userpass <username.workername:password> Stratum login credentials" << endl
			<< "Simulation mode:" << endl
			<< "    -Z [<n>],--simulation [<n>] Mining test mode. Used to validate kernel optimizations. Optionally specify block number." << endl
			<< "Mining configuration:" << endl
			<< "    --opencl-platform <n>  When mining use OpenCL platform n (default: 0)." << endl
			<< "    --opencl-device <n>  When mining use OpenCL device n (default: 0)." << endl
			<< "    --opencl-devices <0 1 ..n> Select which OpenCL devices to mine on. Default is to use all" << endl
			//<< "    -t, --mining-threads <n> Limit number of CPU/GPU miners to n (default: use everything available on selected platform)" << endl
			<< "    --list-devices List the detected OpenCL/CUDA devices and exit." << endl
#if ETH_ETHASHCL
			<< "    --cl-local-work Set the OpenCL local work size. Default is " << OpenCLMiner::c_defaultLocalWorkSize << endl
			<< "    --cl-global-work Set the OpenCL global work size as a multiple of the local work size. Default is " << OpenCLMiner::c_defaultGlobalWorkSizeMultiplier << " * " << OpenCLMiner::c_defaultLocalWorkSize << endl
			//<< "    --cl-parallel-hash <1 2 ..8> Define how many threads to associate per hash. Default=8" << endl
#endif
#if ETH_ETHASHCUDA
			<< " CUDA configuration:" << endl
			<< "    --cuda-block-size Set the CUDA block work size. Default is " << std::to_string(CUDAMiner::c_defaultBlockSize) << endl
			<< "    --cuda-grid-size Set the CUDA grid size. Default is " << std::to_string(CUDAMiner::c_defaultGridSize) << endl
			<< "    --cuda-streams Set the number of CUDA streams. Default is " << std::to_string(CUDAMiner::c_defaultNumStreams) << endl
			<< "    --cuda-schedule <mode> Set the schedule mode for CUDA threads waiting for CUDA devices to finish work. Default is 'sync'. Possible values are:" << endl
			<< "        auto  - Uses a heuristic based on the number of active CUDA contexts in the process C and the number of logical processors in the system P. If C > P, then yield else spin." << endl
			<< "        spin  - Instruct CUDA to actively spin when waiting for results from the device." << endl
			<< "        yield - Instruct CUDA to yield its thread when waiting for results from the device." << endl
			<< "        sync  - Instruct CUDA to block the CPU thread on a synchronization primitive when waiting for the results from the device." << endl
			<< "    --cuda-devices <0 1 ..n> Select which CUDA GPUs to mine on. Default is to use all" << endl
			<< "    --cuda-parallel-hash <1 2 ..8> Define how many hashes to calculate in a kernel, can be scaled to achieve better performance. Default=4" << endl
			<< "    --cuda-noeval  bypass host software re-evaluation of GPU solutions." << endl
			<< "    -L, --dag-load-mode <mode> DAG generation mode." << endl
			<< "        This will trim some milliseconds off the time it takes to send a result to the pool." << endl
			<< "        Use at your own risk! If GPU generates errored results they WILL be forwarded to the pool" << endl
			<< "        Not recommended at high overclock." << endl
#endif
			;
	}

private:
    void doSimulation(int difficulty = 20);

    /*
       doMiner function starts Plant and in farm it starts miners intended to mine e.g. CPUMiner And/or Gpuminer
       doMiner runs a loop where,
           1. it calls getblocktemplate to get the new work
           2. Prepares a Work object, which essentially contains data to be mined after parsing getblocktemplate.
           3. Since there is no queue concept here because of no need of mining old set of transactions to create new block
              Simply reset the Work everytime we get new data
              So doMiner sets work on Plant which internally is meant to forward to all Miners.
           4. One way is to let Plant make a call back as soon as data is ready by using proper synchronization techniques e.g. events
           5. Other is to make a good guess of a few milliseconds wait to check for next getblocktemplate if is different

           What goes inside plant
           6. Inside Plant
              a. Plant runs the miners which is also called as a worker, and a worker spawns a thread to do its job and listen for new work
              b. As soon as plant gets new Work, it sets the data for all miners and expects miners to restart ASAP on new data.
           Basically by requesting them to finish ASAP old block being mined as there is no point.
           Miners drop existing work and take new Work and start mining fresh.
           As soon as a miner finds a solution, it calls plant onSolutionFind and plant expects it to be picked up on next check in doMiner.

           7. Threading design
           8. doMiner runs in main thread
              - Plant starts miners (miner is a worker)
              - miner instead spawns a child thread to do the real mine work and also keep ears open to listen for new data.
              - we need to synchronize plant and miner communication

    */
    void doMiner();

private:
	/// Operating mode.
	OperationMode mode;

	/// Mining options
	bool should_mine = true;
	MinerExecutionMode m_minerExecutionMode = MinerExecutionMode::kCL ;

	unsigned m_openclPlatform = 0;
	unsigned m_miningThreads = 1;
	bool m_shouldListDevices = false;

#if ETH_ETHASHCL
	unsigned m_openclDeviceCount = 0;
	unsigned m_openclDevices[16];
	unsigned m_openclThreadsPerHash = 8;

	unsigned m_globalWorkSizeMultiplier = energi::OpenCLMiner::c_defaultGlobalWorkSizeMultiplier;
	unsigned m_localWorkSize = energi::OpenCLMiner::c_defaultLocalWorkSize;
#endif
#if ETH_ETHASHCUDA
	unsigned m_cudaDeviceCount = 0;
	vector<unsigned> m_cudaDevices = vector<unsigned>(MAX_MINERS, -1);
	unsigned m_numStreams = CUDAMiner::c_defaultNumStreams;
	unsigned m_cudaSchedule = 4; // sync
	unsigned m_cudaGridSize = CUDAMiner::c_defaultGridSize;
	unsigned m_cudaBlockSize = CUDAMiner::c_defaultBlockSize;
	bool m_cudaNoEval = false;
	unsigned m_parallelHash    = 4;
	unsigned m_dagLoadMode = 0; // parallel
#endif

	unsigned m_dagCreateDevice = 0;
    bool m_exit = false;

	/// Benchmarking params
	unsigned m_benchmarkWarmup = 15;
	unsigned m_benchmarkTrial = 3;
	unsigned m_benchmarkTrials = 5;
	unsigned m_benchmarkBlock = 0;


	/// Farm params
	bool m_farmRecheckSet = false;
	int m_worktimeout = 180;
	unsigned m_farmRetries = 0;
	unsigned max_retries_ = 20;
	unsigned m_farmRecheckPeriod = 500;
	unsigned m_defaultStratumFarmRecheckPeriod = 2000;
    std::string m_farmURL = "http://192.168.0.22:9998";
    std::string m_user;
    std::string m_pass;
	std::string m_farmFailOverURL = "";
	std::string m_energiURL = m_farmURL;
	std::string m_fport = "";
	std::string coinbase_addr_;
};
