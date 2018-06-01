#include <memory>

#include "MinerAux.h"
#include <protocol/PoolManager.h>
#include <protocol/stratum/StratumClient.h>
#include <protocol/getwork/GetworkClient.h>

bool MinerCLI::g_running = false;

struct MiningChannel: public LogChannel
{
	static const char* name() { return EthGreen "  m"; }
	static const int verbosity = 2;
	static const bool debug = false;
};

#define minelog clog(MiningChannel)

bool MinerCLI::interpretOption(int& i, int argc, char** argv)
{
    std::string arg = argv[i];
    if ((arg == "--work-timeout") && i + 1 < argc) {
        try {
            m_worktimeout = stoi(argv[++i]);
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }

    } else if ((arg == "-SE" || arg == "--stratum-email") && i + 1 < argc) {
        try {
            m_email = string(argv[++i]);
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    } else if (arg == "--farm-recheck" && i + 1 < argc) {
        try {
            m_farmRecheckSet = true;
            m_farmRecheckPeriod = stol(argv[++i]);
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
        }
    } else if (arg == "--farm-retries" && i + 1 < argc) {
        try {
            m_maxFarmRetries = stol(argv[++i]);
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    } else if (arg == "--display-interval" && i + 1 < argc) {
        try {
            m_displayInterval = stol(argv[++i]);
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    } else if ((arg == "--response-timeout") && i + 1 < argc) {
        try {
            m_responsetimeout = stoi(argv[++i]);
            // Do not allow less than 2 seconds
            // or we may keep disconnecting and reconnecting
            m_responsetimeout = (m_responsetimeout < 2 ? 2 : m_responsetimeout);
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }

    } else if ((arg == "-RH" || arg == "--report-hashrate")) {
        m_report_stratum_hashrate = true;
    } else if (arg == "-HWMON") {
        m_show_hwmonitors = true;
        if ((i + 1 < argc) && (*argv[i + 1] != '-')) {
            try {
                m_show_power = stoul(argv[++i]) != 0;
            } catch (...) {
                cerr << "Bad " << arg << " option: " << argv[i] << endl;
            }
        }
    } else if ((arg == "--coinbase-addr") && i + 1 < argc) {
        coinbase_addr_ = argv[++i];
    } else if (arg == "--farm-recheck" && i + 1 < argc) {
        try {
            m_farmRecheckSet = true;
            m_farmRecheckPeriod = stol(argv[++i]);
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    }
#if ETH_ETHASHCL
    else if (arg == "--opencl-platform" && i + 1 < argc) {
        try {
            m_openclPlatform = stol(argv[++i]);
        }
        catch (...)
        {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    } else if (arg == "--opencl-devices" || arg == "--opencl-device") {
        while (m_openclDeviceCount < 16 && i + 1 < argc) {
            try {
                m_openclDevices[m_openclDeviceCount] = stol(argv[++i]);
                ++m_openclDeviceCount;
            } catch (...) {
                i--;
                break;
            }
        }
    //} else if(arg == "--cl-parallel-hash" && i + 1 < argc) {
    //    try {
    //        m_openclThreadsPerHash = stol(argv[++i]);
    //        if(m_openclThreadsPerHash != 1 && m_openclThreadsPerHash != 2 &&
    //                m_openclThreadsPerHash != 4 && m_openclThreadsPerHash != 8) {
    //            throw;
    //        }
    //    } catch(...) {
    //        cerr << "Bad " << arg << " option: " << argv[i] << endl;
    //        throw;
    //    }
    } else if ((arg == "--cl-global-work" || arg == "--cuda-grid-size")  && i + 1 < argc) {
        try {
            m_globalWorkSizeMultiplier = stoi(argv[++i]);
        }  catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    } else if ((arg == "--cl-local-work" || arg == "--cuda-block-size") && i + 1 < argc) {
        try {
            m_localWorkSize = stol(argv[++i]);
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    }
#endif
#if ETH_ETHASHCL || ETH_ETHASHCUDA
     else if (arg == "--list-devices") {
        m_shouldListDevices = true;
     }
#endif
#if ETH_ETHASHCUDA
     else if (arg == "--cuda-grid-size" && i + 1 < argc) {
         try {
             m_cudaGridSize = stol(argv[++i]);
         } catch (...) {
             cerr << "Bad " << arg << " option: " << argv[i] << endl;
             throw;
         }
     } else if (arg == "--cuda-block-size" && i + 1 < argc) {
         try {
             m_cudaBlockSize = stol(argv[++i]);
         } catch (...) {
             cerr << "Bad " << arg << " option: " << argv[i] << endl;
             throw;
         }
     } else if (arg == "--cuda-devices") {
         while (m_cudaDeviceCount < 16 && i + 1 < argc) {
             try {
                 m_cudaDevices[m_cudaDeviceCount] = stol(argv[++i]);
                 ++m_cudaDeviceCount;
             } catch (...) {
                 --i;
                 break;
             }
         }
     } else if (arg == "--cuda-parallel-hash" && i + 1 < argc) {
         try {
             m_parallelHash = stol(argv[++i]);
             if (m_parallelHash == 0 || m_parallelHash > 8) {
                 throw;
             }
         } catch(...) {
             cerr << "Bad " << arg << " option: " << argv[i] << endl;
             throw;
         }
     } else if (arg == "--cuda-schedule" && i + 1 < argc) {
         std::string mode = argv[++i];
         if (mode == "auto")
             m_cudaSchedule = 0;
         else if (mode == "spin")
             m_cudaSchedule = 1;
         else if (mode == "yield")
             m_cudaSchedule = 2;
         else if (mode == "sync")
             m_cudaSchedule = 4;
         else {
             cerr << "Bad " << arg << "option: " << argv[i] << endl;
             throw;
         }
     } else if (arg == "--cuda-streams" && i + 1 < argc) {
         m_numStreams = stol(argv[++i]);
     } else if (arg == "--cuda-noeval") {
         m_cudaNoEval = true;
     } else if ((arg == "-L" || arg == "--dag-load-mode") && i + 1 < argc) {
         string mode = argv[++i];
         if (mode == "parallel") {
             m_dagLoadMode = DAG_LOAD_MODE_PARALLEL;
         } else if (mode == "sequential") {
             m_dagLoadMode = DAG_LOAD_MODE_SEQUENTIAL;
         } else if (mode == "single") {
             m_dagLoadMode = DAG_LOAD_MODE_SINGLE;
             try {
                 m_dagCreateDevice = stol(argv[++i]);
             } catch (...) {
                 cerr << "Bad " << arg << " option: " << argv[i] << endl;
                 --i;
                 throw;
             }
         } else {
             cerr << "Bad " << arg << " option: " << argv[i] << endl;
             throw;
         }
     }
#endif
     else if ((arg == "--exit")) {
         m_exit = true;
     } else if (arg == "--benchmark-warmup" && i + 1 < argc) {
        try {
            m_benchmarkWarmup = stol(argv[++i]);
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    } else if (arg == "--benchmark-trial" && i + 1 < argc) {
        try {
            m_benchmarkTrial = stol(argv[++i]);
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    } else if (arg == "--benchmark-trials" && i + 1 < argc) {
        try {
            m_benchmarkTrials = stol(argv[++i]);
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    } else if (arg == "-G" || arg == "--opencl") {
        m_minerExecutionMode = MinerExecutionMode::kCL;
    } else if (arg == "-U" || arg == "--cuda") {
        m_minerExecutionMode = MinerExecutionMode::kCUDA;
    } else if (arg == "-X" || arg == "--cuda-opencl") {
        m_minerExecutionMode = MinerExecutionMode::kMixed;
    } else if (arg == "-M" || arg == "--benchmark") {
        m_mode = OperationMode::Benchmark;
        if (i + 1 < argc) {
            std::string m = boost::to_lower_copy(string(argv[++i]));
            try {
                m_benchmarkBlock = stol(m);
            } catch (...) {
                if (argv[i][0] == 45) { // check next arg
                    i--;
                } else {
                    cerr << "Bad " << arg << " option: " << argv[i] << endl;
                    throw;
                }
            }
        }
    } else if (arg == "-Z" || arg == "--simulation") {
        m_mode = OperationMode::Simulation;
        if (i + 1 < argc) {
            string m = boost::to_lower_copy(string(argv[++i]));
            try {
                m_benchmarkBlock = stol(m);
            } catch (...) {
                if (argv[i][0] == 45) { // check next arg
                    i--;
                } else {
                    cerr << "Bad " << arg << " option: " << argv[i] << endl;
                    throw;
                }
            }
        }
    } else if ((arg == "-P") && (i + 1 < argc)) {
        std::string url = argv[++i];
        if (url == "exit") { // add fake scheme and port to 'exit' url
            url = "stratum+tcp://-:x@exit:0";
        }
        URI uri;
        try {
            uri = url;
        } catch (...) {
            cerr << "Bad endpoint address: " << url << endl;
            throw;
        } if (!uri.KnownScheme()) {
            cerr << "Unknown URI scheme " << uri.Scheme() << endl;
            throw;
        }
        m_endpoints.push_back(uri);

        OperationMode mode = OperationMode::None;
        switch (uri.Family()) {
            case ProtocolFamily::STRATUM:
                mode = OperationMode::Stratum;
                break;
            case ProtocolFamily::GETWORK:
                mode = OperationMode::GBT;
                break;
        }
        m_mode = mode;
    //} else if ((arg == "-t" || arg == "--mining-threads") && i + 1 < argc) {
    //    try {
    //        m_miningThreads = stol(argv[++i]);
    //    } catch (...) {
    //        cerr << "Bad " << arg << " option: " << argv[i] << endl;
    //        throw;
    //    }
    } else if ((arg == "--tstop") && i + 1 < argc) {
        try {
            m_tstop = stoul(argv[++i]);
            if (m_tstop != 0 && (m_tstop < 30 || m_tstop > 100)) {
                cerr << "Bad " << arg << " option: " << argv[i] << endl;
                throw;
            }
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    } else if ((arg == "--tstart") && i + 1 < argc) {
        try {
            m_tstart = stoul(argv[++i]);
            if (m_tstart < 30 || m_tstart > 100) {
                cerr << "Bad " << arg << " option: " << argv[i] << endl;
                throw;
            }
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    } else {
        return false;
    }
    // Sanity check --tstart/--tstop
    if (m_tstop && m_tstop <= m_tstart) {
        cerr << "--tstop must be greater than --tstart !" << endl;
        throw;
    }
    if (m_tstop && !m_show_hwmonitors) {
        // if we want stop mining at a specific temperature, we have to
        // monitor the temperature ==> so auto enable HWMON.
        m_show_hwmonitors = true;
    }
    return true;
}

void MinerCLI::execute()
{
    if (m_shouldListDevices) {
#if ETH_ETHASHCL
        if (m_minerExecutionMode == MinerExecutionMode::kCL ||
                m_minerExecutionMode == MinerExecutionMode::kMixed) {
            OpenCLMiner::listDevices();
#endif
#if ETH_ETHASHCUDA
        if (m_minerExecutionMode == MinerExecutionMode::kCUDA ||
                m_minerExecutionMode == MinerExecutionMode::kMixed) {
            CUDAMiner::listDevices();
#endif
            stop_io_service();
            return;
        }
    }

    if (m_minerExecutionMode == MinerExecutionMode::kCL ||
            m_minerExecutionMode == MinerExecutionMode::kMixed) {
# if ETH_ETHASHCL
        if (m_openclDeviceCount > 0) {
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
            stop_io_service();
            exit(1);
        }

        OpenCLMiner::setNumInstances(m_miningThreads);
#else
        cerr << "Selected GPU mining without having compiled with -DETHASHCL=1" << endl;
        exit(1);
#endif
    }

    if (m_minerExecutionMode == MinerExecutionMode::kCUDA ||
            m_minerExecutionMode == MinerExecutionMode::kMixed) {
#if ETH_ETHASHCUDA
        if (m_cudaDeviceCount > 0) {
            CUDAMiner::setDevices(m_cudaDevices, m_cudaDeviceCount);
            m_miningThreads = m_cudaDeviceCount;
        }

        CUDAMiner::setNumInstances(m_miningThreads);
        if (!CUDAMiner::configureGPU(
                    m_cudaBlockSize,
                    m_cudaGridSize,
                    m_numStreams,
                    m_cudaSchedule,
                    m_dagLoadMode,
                    m_dagCreateDevice,
                    m_cudaNoEval,
                    m_exit
                    )) {
            stop_io_service();
            exit(1);
        }
        CUDAMiner::setParallelHash(m_parallelHash);
#else
        cerr << "CUDA support disabled. Configure project build with -DETHASHCUDA=ON" << endl;
        stop_io_service();
        exit(1);
#endif
    }

    g_running = true;
    signal(SIGINT, MinerCLI::signalHandler);
    signal(SIGTERM, MinerCLI::signalHandler);

    if (m_mode == OperationMode::Benchmark) {
        //doBenchmark(m_minerExecutionMode, m_benchmarkWarmup, m_benchmarkTrial, m_benchmarkTrials);
    } else if (m_mode == OperationMode::GBT || m_mode == OperationMode::Stratum) {
        doMiner();
    } else if (m_mode == OperationMode::Simulation) {
        //client = new SimulateClent(20, m_benchmarkBlock);
        //doSimulation();
    } else {
        cerr << "No mining mode selected!" << std::endl;
        exit(1);
    }
}

void MinerCLI::doSimulation(int difficulty)
{
//    auto vEngineModes = getEngineModes(m_minerExecutionMode);
//
//    for( auto mode : vEngineModes ) {
//        cnote << "Starting Miner Engine: " << to_string(mode);
//    }
//    std::mutex mutex_solution;
//    bool solution_found = false;
//    energi::Solution solution;
//
//    SolutionFoundCallback solution_found_cb = [&solution_found, &mutex_solution, &solution](const energi::Solution& found_solution)
//    {
//        std::lock_guard<std::mutex> lock(mutex_solution);
//        solution = found_solution;
//        solution_found = true;
//    };
//
//    // Use Test Miner
//    {
//        energi::MinePlant plant(solution_found_cb);
//        if ( !plant.start({EnumMinerEngine::kTest}) ) {
//            return;
//        }
//
//        energi::SimulatedWork new_work;
//        plant.setWork(new_work);
//
//        solution_found = false;
//        while(!solution_found) {
//            auto mp = plant.miningProgress();
//            mp.rate();
//
//            this_thread::sleep_for(chrono::milliseconds(1000));
//            cnote << "Mining on difficulty " << difficulty << " " << mp;
//        }
//
//        std::lock_guard<std::mutex> lock(mutex_solution);
//        std::cout << "Solution found!!" << std::endl;
//    }
//
//    // Use CPU Miner
//    {
//        energi::MinePlant plant(solution_found_cb);
//        if ( !plant.start({EnumMinerEngine::kCPU}) ) {
//            return;
//        }
//
//        energi::SimulatedWork new_work;
//        plant.setWork(new_work);
//
//        solution_found = false;
//        while(!solution_found) {
//            auto mp = plant.miningProgress();
//            mp.rate();
//
//            this_thread::sleep_for(chrono::milliseconds(1000));
//            cnote << "Mining on difficulty " << difficulty << " " << mp;
//        }
//
//        std::lock_guard<std::mutex> lock(mutex_solution);
//        std::cout << "Solution found!!" << std::endl;
//    }
}

void MinerCLI::doMiner()
{
    PoolClient* client = nullptr;
    if (m_mode == OperationMode::GBT) {
			client = new GetworkClient(m_farmRecheckPeriod, coinbase_addr_);
    } else if (m_mode == OperationMode::Stratum) {
        client = new StratumClient(m_io_service, m_worktimeout, m_responsetimeout, m_email, m_report_stratum_hashrate);
    } else {
        cwarn << "Inwalid OperationMode";
        std::exit(1);
    }
    if (client == nullptr) {
        stop_io_service();
        //! This should not happen
        std::cerr << "Client is not contsructed normally" << std::endl;
        std::exit(1);
    }
    cnote << "Engines started!";
    energi::MinePlant plant(m_io_service);
    PoolManager mgr(client, plant, m_minerExecutionMode, m_maxFarmRetries);

    // If we are in simulation mode we add a fake connection
    if (m_mode == OperationMode::Simulation) {
        URI con(URI("http://-:0"));
        mgr.clearConnections();
        mgr.addConnection(con);
    } else {
        for (auto conn : m_endpoints) {
            mgr.addConnection(conn);
        }
    }
    //start PoolManager
    mgr.start();

    // Run CLI in loop
    while (g_running && mgr.isRunning()) {
        if (mgr.isConnected()) {
            auto mp = plant.miningProgress(m_show_hwmonitors, m_show_power);
            minelog << mp << plant.getSolutionStats() << plant.farmLaunchedFormatted();
        } else {
            minelog << "not-connected";
        }
        std::this_thread::sleep_for(std::chrono::seconds(m_displayInterval));
    }
    mgr.stop();
    stop_io_service();
    exit(0);
}

void MinerCLI::io_work_timer_handler(const boost::system::error_code& ec)
{

    if (!ec) {

        // This does absolutely nothing aside resubmitting timer
        // ensuring io_service's queue has always something to do
        m_io_work_timer.expires_from_now(boost::posix_time::seconds(120));
        m_io_work_timer.async_wait(m_io_strand.wrap(boost::bind(&MinerCLI::io_work_timer_handler, this, boost::asio::placeholders::error)));
    }

}

void MinerCLI::stop_io_service()
{
    // Here we stop all io_service's related activities
    m_io_service.stop();
    m_io_thread.join();

}
