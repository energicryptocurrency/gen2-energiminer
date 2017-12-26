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
#include "FarmClient.h"


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


using namespace std;
using namespace boost::algorithm;
using namespace energi;
using namespace egihash;

enum class MinerExecutionMode : unsigned
{
  kCPU  = 0x1,
  kCL   = 0x2,
  kMixed= 0x3
};




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
			m_energiURL = m_farmURL;
		}
		else if ((arg == "--coinbase-addr") && i + 1 < argc)
		{
			coinbase_addr_ = argv[++i];
		}
		else if (arg == "--farm-recheck" && i + 1 < argc)
			try {
				m_farmRecheckSet = true;
				m_farmRecheckPeriod = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				throw;
			}
		else if (arg == "--farm-retries" && i + 1 < argc)
			try {
				max_retries_ = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
        throw;
			}
		else if (arg == "--opencl-platform" && i + 1 < argc)
			try {
				m_openclPlatform = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
        throw;
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
	        throw;
				} 
			}
			catch(...) {
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
        throw;
			}
		}
		else if ((arg == "--cl-global-work" || arg == "--cuda-grid-size")  && i + 1 < argc)
			try {
				m_globalWorkSizeMultiplier = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
        throw;
			}
		else if ((arg == "--cl-local-work" || arg == "--cuda-block-size") && i + 1 < argc)
			try {
				m_localWorkSize = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				throw;
			}
		else if (arg == "--list-devices")
			m_shouldListDevices = true;
		else if (arg == "--benchmark-warmup" && i + 1 < argc)
			try {
				m_benchmarkWarmup = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				throw;
			}
		else if (arg == "--benchmark-trial" && i + 1 < argc)
			try {
				m_benchmarkTrial = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				throw;
			}
		else if (arg == "--benchmark-trials" && i + 1 < argc)
			try
			{
				m_benchmarkTrials = stol(argv[++i]);
			}
			catch (...)
			{
				cerr << "Bad " << arg << " option: " << argv[i] << endl;
				throw;
			}
		/*else if (arg == "-G" || arg == "--opencl")
			m_MinerExecutionMode = MinerExecutionMode::CL;
		else if (arg == "-X" || arg == "--cuda-opencl")
		{
			m_MinerExecutionMode = MinerExecutionMode::Mixed;
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
						throw;
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
						throw;
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
				throw;
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
			if (m_MinerExecutionMode == MinerExecutionMode::kCL || m_MinerExecutionMode == MinerExecutionMode::kMixed)
			{
				OpenCLMiner::listDevices();
				exit(0);
			}
		}

		if (m_MinerExecutionMode == MinerExecutionMode::kCL || m_MinerExecutionMode == MinerExecutionMode::kMixed)
		{
			if (m_openclDeviceCount > 0)
			{
				OpenCLMiner::setDevices(m_openclDevices, m_openclDeviceCount);
				m_miningThreads = m_openclDeviceCount;
			}
			
			OpenCLMiner::setThreadsPerHash(m_openclThreadsPerHash);
			if (!OpenCLMiner::configureGPU(
                    m_localWorkSize,
                    m_globalWorkSizeMultiplier,
                    m_openclPlatform,
                    0))
			{
				exit(1);
			}

			OpenCLMiner::setNumInstances(m_miningThreads);
		}

		switch(mode)
		{
		  case OperationMode::Benchmark:
		    ///doBenchmark(m_MinerExecutionMode, m_benchmarkWarmup, m_benchmarkTrial, m_benchmarkTrials);
		    break;
		  case OperationMode::GBT:
		    doGBT(m_MinerExecutionMode, m_energiURL, m_farmRecheckPeriod);
        break;
		  case OperationMode::Simulation:
		    doSimulation(m_MinerExecutionMode);
        break;
		  default:
		    cerr << "No mining mode selected!" << std::endl;
        exit(-1);
		    break;
		}
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
			<< "    --cl-local-work Set the OpenCL local work size. Default is " << OpenCLMiner::c_defaultLocalWorkSize << endl
			<< "    --cl-global-work Set the OpenCL global work size as a multiple of the local work size. Default is " << OpenCLMiner::c_defaultGlobalWorkSizeMultiplier << " * " << OpenCLMiner::c_defaultLocalWorkSize << endl
			<< "    --cl-parallel-hash <1 2 ..8> Define how many threads to associate per hash. Default=8" << endl
			;
	}

private:


  std::vector<energi::EnumMinerEngine> getEngineModes(MinerExecutionMode minerExecutionMode)
  {
    std::vector<energi::EnumMinerEngine> vEngine;
    if ( static_cast<unsigned>(minerExecutionMode) & static_cast<unsigned>(MinerExecutionMode::kCL) )
      vEngine.push_back(energi::EnumMinerEngine::kCL);
    if ( static_cast<unsigned>(minerExecutionMode) & static_cast<unsigned>(MinerExecutionMode::kCPU) )
      vEngine.push_back(energi::EnumMinerEngine::kCPU);

    return vEngine;
  }

	void doSimulation(MinerExecutionMode minerExecutionMode, int difficulty = 20)
  {
	  auto vEngineModes = getEngineModes(minerExecutionMode);

	  for( auto mode : vEngineModes )
	    cnote << "Starting Miner Engine: " << energi::to_string(mode);

    std::mutex mutex_solution;
    bool solution_found = false;
    energi::Solution solution;

    SolutionFoundCallback solution_found_cb = [&solution_found, &mutex_solution, &solution](const energi::Solution& found_solution)
    {
      MutexLGuard l(mutex_solution);
      solution = found_solution;
      solution_found = true;
    };

    // Use Test Miner
    {
      energi::MinePlant plant(solution_found_cb);
      if ( !plant.start({energi::EnumMinerEngine::kTest}) )
      {
        return;
      }

      energi::SimulatedWork new_work;
      plant.setWork(new_work);

      solution_found = false;
      while(!solution_found)
      {
        auto mp = plant.miningProgress();
        mp.rate();

        this_thread::sleep_for(chrono::milliseconds(1000));
        cnote << "Mining on difficulty " << difficulty << " " << mp;
      }

      MutexLGuard l(mutex_solution);
      std::cout << "Solution found!!" << std::endl;
    }

    // Use CPU Miner
    {
      energi::MinePlant plant(solution_found_cb);
      if ( !plant.start({energi::EnumMinerEngine::kCPU}) )
      {
        return;
      }

      energi::SimulatedWork new_work;
      plant.setWork(new_work);

      solution_found = false;
      while(!solution_found)
      {
        auto mp = plant.miningProgress();
        mp.rate();

        this_thread::sleep_for(chrono::milliseconds(1000));
        cnote << "Mining on difficulty " << difficulty << " " << mp;
      }

      MutexLGuard l(mutex_solution);
      std::cout << "Solution found!!" << std::endl;
    }

    // Whoever finds first exit
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


	void doGBT(MinerExecutionMode minerExecutionMode, string & _remote, unsigned _recheckPeriod)
	{
    (void)_remote;
    (void)_recheckPeriod;

    jsonrpc::HttpClient client(m_farmURL);
    GBTClient rpc(client);

    // Start plant now with given miners
    // start plant full of miners
    std::mutex mutex_solution;
    bool solution_found = false;
    energi::Solution solution;

    // Note, this is mostly called from a miner thread, but since solution is consumed in main thread after set
    // its safe to not lock the access
    SolutionFoundCallback solution_found_cb = [&solution_found, &mutex_solution, &solution](const energi::Solution& found_solution)
    {
      MutexLGuard l(mutex_solution);
      solution = found_solution;
      solution_found = true;
    };

    // Create plant
    energi::MinePlant plant(solution_found_cb);
    //if ( !plant.start({ { "cpu", 1 }, { "cl", OpenCLMiner::instances() } }) )
    //if ( !plant.start({ { "cl", OpenCLMiner::instances() } }) )
    auto vEngineModes = getEngineModes(minerExecutionMode);
    if ( !plant.start(vEngineModes) )
    {
      return;
    }


    cnote << "Engines started!";

    energi::Work current_work;
    // Mine till you can, or retries fail after a limit
    while (should_mine)
		{
			try
			{
			  solution_found = false;
        // Keep checking for new work and mine
        while(!solution_found)
        {
          // Get Work using GetBlockTemplate
          auto work_gbt = rpc.getBlockTemplate();
          energi::Work new_work(work_gbt, coinbase_addr_);
          // check if current work is no different, then skip
          if ( new_work != current_work )
          {
            cnote << "work submitted";
            // 1. Got new work
            // 2. Abandon current work and take new work
            // 3. miner starts mining for new work

            current_work = new_work;
            plant.setWork(new_work);

            // 4. Work has been assigned to the plant
            // 5. Wait and continue for new work
            // 6. TODO decide on time to wait for
          }

          auto mp = plant.miningProgress();
          mp.rate();

          cnote << "Mining on difficulty " << " " << mp;

          this_thread::sleep_for(chrono::milliseconds(500));
        }

        // 7. Since solution was found, before submit stop all miners
        plant.stopAllWork();

        // 8. Now submit
        MutexLGuard l(mutex_solution);
        rpc.submitSolution(solution);
        current_work.reset();
			}
			catch(WorkException &we)
			{
        if (max_retries_ == 0)
        {
          cerr << "Work decode problem, will exit now" << endl;
          should_mine = false;
        }
        else
        {
          for (auto i = 3; --i; this_thread::sleep_for(chrono::seconds(1)))
            cerr << we.what() << endl << "Work couldn't be decoded, possible json parsing problem." << i << "... \r";
          cerr << endl;
        }

        --max_retries_;
			}
			catch (jsonrpc::JsonRpcException& je)
			{
				if (max_retries_ == 0)
				{
					cerr << "JSON-RPC problem. Couldn't connect, will exit now" << endl;
					should_mine = false;
				}
				else
				{
					for (auto i = 3; --i; this_thread::sleep_for(chrono::seconds(1)))
						cerr << je.GetMessage() << endl << je.what() << endl << "JSON-RPC problem. Probably couldn't connect. Retrying in " << i << "... \r";
					cerr << endl;
				}

				--max_retries_;
			}
		}

		cout << "Hello world: " << m_farmURL << " " << endl;
		exit(0);
	}


	/// Operating mode.
	OperationMode mode;

	/// Mining options
	bool should_mine = true;
	MinerExecutionMode m_MinerExecutionMode = MinerExecutionMode::kCPU;

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
	string m_farmURL = "http://192.168.0.22:9998";
	string m_farmFailOverURL = "";
	string m_energiURL = m_farmURL;
	unsigned m_farmRetries = 0;
	unsigned max_retries_ = 20;
	unsigned m_farmRecheckPeriod = 500;
	unsigned m_defaultStratumFarmRecheckPeriod = 2000;
	bool m_farmRecheckSet = false;
	int m_worktimeout = 180;
	string m_fport = "";
	string coinbase_addr_;
};
