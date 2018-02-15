#include "MinerAux.h"
#include "protocol/StratumClient.h"
#include "protocol/GBTClient.h"

bool MinerCLI::interpretOption(int& i, int argc, char** argv)
{
    string arg = argv[i];
    if ((arg == "--gbt") && i + 1 < argc) {
        mode = OperationMode::GBT;
        m_farmURL = argv[++i];
        m_energiURL = m_farmURL;
    } else if ((arg == "-S" || arg == "--stratum") && (i + 1 < argc)) {
        mode = OperationMode::Stratum;
        m_farmURL = argv[++i];
        m_energiURL = m_farmURL;
    } else if ((arg == "-FO" || arg == "--failover-userpass") && i + 1 < argc) {
        mode = OperationMode::Stratum;
        m_farmFailOverURL = argv[++i];
    } else if ((arg == "-SP" || arg == "--stratum-protocol") && i + 1 < argc) {
        try {
            m_stratumProtocol = atoi(argv[++i]);
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
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
    } else if (arg == "--farm-retries" && i + 1 < argc) {
        try {
            max_retries_ = stol(argv[++i]);
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    } else if (arg == "--opencl-platform" && i + 1 < argc) {
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
    } else if(arg == "--cl-parallel-hash" && i + 1 < argc) {
        try {
            m_openclThreadsPerHash = stol(argv[++i]);
            if(m_openclThreadsPerHash != 1 && m_openclThreadsPerHash != 2 &&
                    m_openclThreadsPerHash != 4 && m_openclThreadsPerHash != 8) {
                throw;
            }
        } catch(...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    } else if ((arg == "--cl-global-work" || arg == "--cuda-grid-size")  && i + 1 < argc) {
        try {
            m_globalWorkSizeMultiplier = stol(argv[++i]);
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
    } else if (arg == "--list-devices") {
        m_shouldListDevices = true;
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
    }
    /*else if (arg == "-X" || arg == "--cuda-opencl")
      {
      m_minerExecutionMode = MinerExecutionMode::Mixed;
      }*/
    else if (arg == "-M" || arg == "--benchmark") {
        mode = OperationMode::Benchmark;
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
    } else if (arg == "-Z" || arg == "--simulation") {
        mode = OperationMode::Simulation;
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
    } else if ((arg == "-t" || arg == "--mining-threads") && i + 1 < argc) {
        try {
            m_miningThreads = stol(argv[++i]);
        } catch (...) {
            cerr << "Bad " << arg << " option: " << argv[i] << endl;
            throw;
        }
    } else {
        return false;
    }
    return true;
}

void MinerCLI::execute()
{
    if (m_shouldListDevices) {
        if (m_minerExecutionMode == MinerExecutionMode::kCL ||
                m_minerExecutionMode == MinerExecutionMode::kMixed) {
            OpenCLMiner::listDevices();
            return;
        }
    }

    if (m_minerExecutionMode == MinerExecutionMode::kCL) {
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
            exit(1);
        }

        OpenCLMiner::setNumInstances(m_miningThreads);
    }

    switch(mode)
    {
        case OperationMode::Benchmark:
            ///doBenchmark(m_minerExecutionMode, m_benchmarkWarmup, m_benchmarkTrial, m_benchmarkTrials);
            break;
        case OperationMode::GBT:
            doGBT();
            break;
        case OperationMode::Simulation:
            doSimulation();
            break;
        case OperationMode::Stratum:
            doStratum();
            break;
        default:
            cerr << "No mining mode selected!" << std::endl;
            exit(-1);
            break;
    }
}

void MinerCLI::doSimulation(int difficulty)
{
    auto vEngineModes = getEngineModes(m_minerExecutionMode);

    for( auto mode : vEngineModes ) {
        cnote << "Starting Miner Engine: " << to_string(mode);
    }
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
        if ( !plant.start({EnumMinerEngine::kTest}) ) {
            return;
        }

        energi::SimulatedWork new_work;
        plant.setWork(new_work);

        solution_found = false;
        while(!solution_found) {
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
        if ( !plant.start({EnumMinerEngine::kCPU}) ) {
            return;
        }

        energi::SimulatedWork new_work;
        plant.setWork(new_work);

        solution_found = false;
        while(!solution_found) {
            auto mp = plant.miningProgress();
            mp.rate();

            this_thread::sleep_for(chrono::milliseconds(1000));
            cnote << "Mining on difficulty " << difficulty << " " << mp;
        }

        MutexLGuard l(mutex_solution);
        std::cout << "Solution found!!" << std::endl;
    }
}

void MinerCLI::doStratum()
{
    if (m_farmRecheckSet) {
        m_farmRecheckPeriod = m_defaultStratumFarmRecheckPeriod;
    }
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
    StratumClient client(&plant, m_minerExecutionMode, m_farmURL, max_retries_, m_worktimeout);
    auto vEngineModes = getEngineModes(m_minerExecutionMode);
    if ( !plant.start(vEngineModes) ) {
        return;
    }
    if (!m_farmFailOverURL.empty()) {
        client.setFailover(m_farmFailOverURL);
    }
    cnote << "Engines started!";
    while (client.isRunning()) {
        auto mp = plant.miningProgress();
        if (client.isConnected()) {
            if (client.current()) {
                cnote << "Mining on difficulty " << " " << mp;
            } else {
                cnote << "Waiting for work package...";
            }
            //!TODO
            //client.submitHashrate(mp.rate());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(m_farmRecheckPeriod));
    }
}

void MinerCLI::doGBT()
{
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
    auto vEngineModes = getEngineModes(m_minerExecutionMode);
    if ( !plant.start(vEngineModes) ) {
        return;
    }
    cnote << "Engines started!";

    energi::Work current_work;
    // Mine till you can, or retries fail after a limit
    while (should_mine) {
        try {
            solution_found = false;
            // Keep checking for new work and mine
            while(!solution_found) {
                // Get Work using GetBlockTemplate
                auto work_gbt = rpc.getBlockTemplate();
                energi::Work new_work(work_gbt, coinbase_addr_);
                // check if current work is no different, then skip
                if ( new_work != current_work ) {
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
        } catch(WorkException &we) {
            if (max_retries_ == 0) {
                cerr << "Work decode problem, will exit now" << endl;
                should_mine = false;
            } else {
                for (auto i = 3; --i; this_thread::sleep_for(chrono::seconds(1)))
                    cerr << we.what() << endl << "Work couldn't be decoded, possible json parsing problem." << i << "... \r";
                cerr << endl;
            }
            --max_retries_;
        } catch (jsonrpc::JsonRpcException& je) {
            if (max_retries_ == 0) {
                cerr << "JSON-RPC problem. Couldn't connect, will exit now" << endl;
                should_mine = false;
            } else {
                for (auto i = 3; --i; this_thread::sleep_for(chrono::seconds(1)))
                    cerr << je.GetMessage() << endl << je.what() << endl << "JSON-RPC problem. Probably couldn't connect. Retrying in " << i << "... \r";
                cerr << endl;
            }
            --max_retries_;
        }
    }
    cout << "GBT simulation is exiting: " << m_farmURL << " " << endl;
    plant.stopAllWork();
    return;
}
