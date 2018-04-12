#include <memory>

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
    } else if ((arg == "-O" || arg == "--userpass") && i + 1 < argc) {
        std::string userpass = std::string(argv[++i]);
        size_t p = userpass.find_first_of(":");
        m_user = userpass.substr(0, p);
        if (p + 1 < userpass.length()) {
            m_pass = userpass.substr(p + 1);
        }
    } else if ((arg == "-u" || arg == "--user") && i + 1 < argc) {
        m_user = std::string(argv[++i]);
    } else if ((arg == "-p" || arg == "--pass") && i + 1 < argc) {
        m_pass = std::string(argv[++i]);
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

    if (mode == OperationMode::Benchmark) {
            ///doBenchmark(m_minerExecutionMode, m_benchmarkWarmup, m_benchmarkTrial, m_benchmarkTrials);
    } else if (mode == OperationMode::GBT || mode == OperationMode::Stratum) {
        doMiner();
    } else if (mode == OperationMode::Simulation) {
        doSimulation();
    } else {
        cerr << "No mining mode selected!" << std::endl;
        exit(-1);
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
        std::lock_guard<std::mutex> lock(mutex_solution);
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

        std::lock_guard<std::mutex> lock(mutex_solution);
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

        std::lock_guard<std::mutex> lock(mutex_solution);
        std::cout << "Solution found!!" << std::endl;
    }
}

void MinerCLI::doMiner()
{
    // Start plant now with given miners
    // start plant full of miners
    std::mutex mutex_solution;
    std::atomic_bool solution_found(false);
    energi::Solution solution;
    // Note, this is mostly called from a miner thread, but since solution is consumed in main thread after set
    // its safe to not lock the access
    SolutionFoundCallback solution_found_cb = [&solution_found, &mutex_solution, &solution](const energi::Solution& found_solution)
    {
        std::lock_guard<std::mutex> lock(mutex_solution);
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
    std::unique_ptr<MiningClient> client;
    jsonrpc::HttpClient cli = jsonrpc::HttpClient(m_farmURL);
    if (mode == OperationMode::GBT) {
        client.reset(new GBTClient(cli, coinbase_addr_));
    } else if (mode == OperationMode::Stratum) {
        client.reset(new StratumClient(&plant,
                                        m_minerExecutionMode,
                                        m_farmURL,
                                        m_user,
                                        m_pass,
                                        max_retries_,
                                        m_worktimeout));
    }
    if (client == nullptr) {
        //! This should not happen
        std::cerr << "Client is not contsructed normally" << std::endl;
        std::exit(-1);
    }
    cnote << "Engines started!";

    energi::Work current_work;
    // Mine till you can, or retries fail after a limit
    while (should_mine) {
        try {
            solution_found = false;
            // Keep checking for new work and mine
            while(!solution_found) {
                energi::Work new_work = client->getWork();
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

                this_thread::sleep_for(chrono::milliseconds(500));
            }
            // 7. Since solution was found, before submit stop all miners
            plant.stopAllWork();
            // 8. Now submit
            std::lock_guard<std::mutex> lock(mutex_solution);
            client->submit(solution);
            current_work.reset();
            solution_found = false;
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
    cout << "simulation is exiting: " << m_farmURL << " " << endl;
    plant.stopAllWork();
    return;
}
