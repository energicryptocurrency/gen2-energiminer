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
#include <protocol/PoolURI.h>


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
#include <boost/bind.hpp>

#ifdef NRGHASHCL
#include "libegihash-cl/OpenCLMiner.h"
#endif

#ifdef NRGHASHCUDA
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

	static void signalHandler(int sig)
	{
		(void)sig;
		g_running = false;
	}

	MinerCLI()
        : m_io_work(m_io_service)
        , m_io_work_timer(m_io_service)
        , m_io_strand(m_io_service)
    {
        // Post first deadline timer to give io_service
        // initial work
        m_io_work_timer.expires_from_now(boost::posix_time::seconds(60));
        m_io_work_timer.async_wait(m_io_strand.wrap(boost::bind(&MinerCLI::io_work_timer_handler, this, boost::asio::placeholders::error)));

        // Start io_service in it's own thread
        m_io_thread = std::thread{ boost::bind(&boost::asio::io_service::run, &m_io_service) };

        // Io service is now live and running
        // All components using io_service should post to reference of m_io_service
        // and should not start/stop or even join threads (which heavily time consuming)
    }

	void io_work_timer_handler(const boost::system::error_code& ec);
    void stop_io_service();

    void ParseCommandLine(int argc,char** argv);
	void execute();

private:
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
	OperationMode m_mode;

	/// Global boost's io_service
	std::thread m_io_thread;									// The IO service thread
	boost::asio::io_service m_io_service;						// The IO service itself
	boost::asio::io_service::work m_io_work;					// The IO work which prevents io_service.run() to return on no work thus terminating thread
	boost::asio::deadline_timer m_io_work_timer;				// A dummy timer to keep io_service with something to do and prevent io shutdown
	boost::asio::io_service::strand m_io_strand;				// A strand to serialize posts in multithreaded environment

	/// Mining options
	static bool g_running;
	MinerExecutionMode m_minerExecutionMode = MinerExecutionMode::kCL;

	unsigned m_openclPlatform = 0;
	unsigned m_miningThreads = 1;
	bool m_shouldListDevices = false;

#if NRGHASHCL
	unsigned m_openclDeviceCount = 0;
    std::vector<unsigned> m_openclDevices = std::vector<unsigned>(MAX_MINERS, -1);
	unsigned m_openclThreadsPerHash = 8;

    int m_globalWorkSizeMultiplier = energi::OpenCLMiner::c_defaultGlobalWorkSizeMultiplier;
	unsigned m_localWorkSize = energi::OpenCLMiner::c_defaultLocalWorkSize;
#endif
#if NRGHASHCUDA
	unsigned m_cudaDeviceCount = 0;
	vector<unsigned> m_cudaDevices = vector<unsigned>(MAX_MINERS, -1);
	unsigned m_numStreams = CUDAMiner::c_defaultNumStreams;
	unsigned m_cudaSchedule = 4; // sync
	unsigned m_cudaGridSize = CUDAMiner::c_defaultGridSize;
	unsigned m_cudaBlockSize = CUDAMiner::c_defaultBlockSize;
	unsigned m_cudaParallelHash    = 4;
#endif

	unsigned m_dagLoadMode = 0; // parallel
	bool m_noEval = false;
	unsigned m_dagCreateDevice = 0;
    bool m_exit = false;

	/// Benchmarking params
	unsigned m_benchmarkWarmup = 15;
	unsigned m_benchmarkTrial = 3;
	unsigned m_benchmarkTrials = 5;
	unsigned m_benchmarkBlock = 0;
    std::vector<URI> m_endpoints;


	/// Farm params
	int m_worktimeout = 180;
	// Number of seconds to wait before triggering a response timeout from pool
	int m_responsetimeout = 3;
    // Number of minutes to wait on a failover pool before trying to go back to primary. In minutes !!
    unsigned m_failovertimeout = 0;

	bool m_show_hwmonitors = false;
	bool m_show_power = false;

    unsigned m_tstop = 0;
    unsigned m_tstart = 40;

	unsigned m_maxFarmRetries = 3;
	unsigned m_farmRecheckPeriod = 500;
	unsigned m_displayInterval = 5;
	bool m_farmRecheckSet = false;

	std::string m_coinbase_addr;

	bool m_report_stratum_hashrate = false;
};
