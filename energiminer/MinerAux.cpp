#include <memory>

#include "MinerAux.h"
#include <energiminer/buildinfo.h>
#include <protocol/PoolManager.h>
#include <protocol/stratum/StratumClient.h>
#include <protocol/getwork/GetworkClient.h>

#include <CLI/CLI.hpp>

bool MinerCLI::g_running = false;

struct MiningChannel: public LogChannel
{
	static const char* name() { return EthGreen "  m"; }
	static const int verbosity = 2;
	static const bool debug = false;
};

#define minelog clog(MiningChannel)

void MinerCLI::ParseCommandLine(int argc, char** argv)
{

    const char* CommonGroup = "Common Options";
#if ETH_ETHASHCL
    const char* OpenCLGroup = "OpenCL Options";
#endif
#if ETH_ETHASHCUDA
    const char* CUDAGroup =   "CUDA Options";
#endif
    CLI::App app("Ethminer - GPU Ethereum miner");

    bool help = false;
    app.set_help_flag();
    app.add_flag("-h,--help", help, "Show help")
        ->group(CommonGroup);

    bool vers = false;
    app.add_flag("-V,--version", vers,
            "Show program version")
        ->group(CommonGroup);

    app.add_option("-v,--verbosity", g_logVerbosity,
            "Set log verbosity level", true)
        ->group(CommonGroup)
        ->check(CLI::Range(9));

    app.add_option("--farm-recheck", m_farmRecheckPeriod,
            "Set check interval in ms.for changed work", true)
        ->group(CommonGroup)
        ->check(CLI::Range(1, 99999));

    app.add_option("--farm-retries", m_maxFarmRetries,
            "Set number of reconnection retries", true)
        ->group(CommonGroup)
        ->check(CLI::Range(1, 99999));

    app.add_option("--stratum-email", m_email,
            "Set email address for eth-proxy")
        ->group(CommonGroup);

    app.add_option("--work-timeout", m_worktimeout,
            "Set disconnect timeout in seconds of working on the same job", true)
        ->group(CommonGroup)
        ->check(CLI::Range(1, 99999));

    app.add_option("--response-timeout", m_responsetimeout,
            "Set disconnect timeout in seconds for pool responses", true)
        ->group(CommonGroup)
        ->check(CLI::Range(2, 999));

    app.add_flag("-R,--report-hashrate", m_report_stratum_hashrate,
            "Report current hashrate to pool")
        ->group(CommonGroup);

    app.add_option("--display-interval", m_displayInterval,
            "Set mining stats log interval in seconds", true)
        ->group(CommonGroup)
        ->check(CLI::Range(1, 99999));

    unsigned hwmon;
    auto hwmon_opt = app.add_option("--HWMON", hwmon,
            "0 - Displays gpu temp, fan percent. 1 - and power usage."
            " Note for Linux: The program uses sysfs for power, which requires running with root privileges.");
    hwmon_opt->group(CommonGroup)
        ->check(CLI::Range(1));

    app.add_flag("--exit", m_exit,
            "Stops the miner whenever an error is encountered")
        ->group(CommonGroup);
    std::vector<std::string> pools;
    app.add_option("-P,--pool,pool", pools,
            "Specify one or more pool URLs. See below for URL syntax")
        ->group(CommonGroup);
    app.add_option("--failover-timeout", m_failovertimeout,
            "Set the amount of time in minutes to stay on a failover pool before trying to reconnect to primary. If = 0 then no switch back.", true)
        ->group(CommonGroup)
        ->check(CLI::Range(0, 999));
#if ETH_ETHASHCL || ETH_ETHASHCUDA
    app.add_flag("--list-devices", m_shouldListDevices,
            "List the detected OpenCL/CUDA devices and exit. Should be combined with -G, -U, or -X flag")
        ->group(CommonGroup);
#endif
#if ETH_ETHASHCL
    app.add_option("--opencl-platform", m_openclPlatform,
            "Use OpenCL platform n", true)
        ->group(OpenCLGroup);

    app.add_option("--opencl-device,--opencl-devices", m_openclDevices,
            "Select list of devices to mine on (default: use all available)")
        ->group(OpenCLGroup);

    app.add_set("--cl-parallel-hash", m_openclThreadsPerHash, {1, 2, 4, 8},
            "Set the number of threads per hash", true)
        ->group(OpenCLGroup);

    app.add_option("--cl-global-work", m_globalWorkSizeMultiplier,
            "Set the global work size multipler. Specify negative value for automatic scaling based on # of compute units", true)
        ->group(OpenCLGroup);


    app.add_option("--cl-local-work", m_localWorkSize,
            "Set the local work size", true)
        ->group(OpenCLGroup)
        ->check(CLI::Range(32, 99999));
#endif
#if ETH_ETHASHCUDA
    app.add_option("--cuda-grid-size", m_cudaGridSize,
            "Set the grid size", true)
        ->group(CUDAGroup)
        ->check(CLI::Range(1, 999999999));

    app.add_option("--cuda-block-size", m_cudaBlockSize,
            "Set the block size", true)
        ->group(CUDAGroup)
        ->check(CLI::Range(1, 999999999));

    app.add_option("--cuda-devices", m_cudaDevices,
            "Select list of devices to mine on (default: use all available)")
        ->group(CUDAGroup);

    app.add_set("--cuda-parallel-hash", m_cudaParallelHash, {1, 2, 4, 8},
            "Set the number of hashes per kernel", true)
        ->group(CUDAGroup);

    string sched = "sync";
    app.add_set("--cuda-schedule", sched, {"auto", "spin", "yield", "sync"},

            "Set the scheduler mode."
            "  auto  - Uses a heuristic based on the number of active CUDA contexts in the process C "
            "          and the number of logical processors in the system P. If C > P then yield else spin."
            "  spin  - Instruct CUDA to actively spin when waiting for results from the device."
            "  yield - Instruct CUDA to yield its thread when waiting for results from the device."
            "  sync  - Instruct CUDA to block the CPU thread on a synchronization primitive when waiting for the results from the device."
            "  ", true)
        ->group(CUDAGroup);

    app.add_option("--cuda-streams", m_numStreams,
            "Set the number of streams", true)
        ->group(CUDAGroup)
        ->check(CLI::Range(1, 99));
#endif
    app.add_flag("--noeval", m_noEval,
            "Bypass host software re-evaluation of GPU solutions")
        ->group(CommonGroup);

    app.add_option("-L,--dag-load-mode", m_dagLoadMode,
            "Set the DAG load mode. 0=parallel, 1=sequential, 2=single."
            "  parallel    - load DAG on all GPUs at the same time"
            "  sequential  - load DAG on GPUs one after another. Use this when the miner crashes during DAG generation"
            "  single      - generate DAG on device, then copy to other devices. Implies --dag-single-dev"
            "  ", true)
        ->group(CommonGroup)
        ->check(CLI::Range(2));

    app.add_option("--dag-single-dev", m_dagCreateDevice,
            "Set the DAG creation device in single mode", true)
        ->group(CommonGroup);

    app.add_option("--benchmark-warmup", m_benchmarkWarmup,
            "Set the duration in seconds of warmup for the benchmark tests", true)
        ->group(CommonGroup);

    app.add_option("--benchmark-trial", m_benchmarkTrial,
            "Set the number of benchmark trials to run", true)
        ->group(CommonGroup)
        ->check(CLI::Range(1, 99));

    bool cl_miner = false;
    app.add_flag("-G,--opencl", cl_miner,
            "When mining use the GPU via OpenCL")
        ->group(CommonGroup);

    bool cuda_miner = false;
    app.add_flag("-U,--cuda", cuda_miner,
            "When mining use the GPU via CUDA")
        ->group(CommonGroup);

    bool mixed_miner = false;
    app.add_flag("-X,--cuda-opencl", mixed_miner,
            "When mining with mixed AMD(OpenCL) and CUDA GPUs")
        ->group(CommonGroup);

    auto bench_opt = app.add_option("-M,--benchmark", m_benchmarkBlock,
            "Benchmark mining and exit; Specify block number to benchmark against specific DAG", true);
    bench_opt->group(CommonGroup);

    auto sim_opt = app.add_option("-Z,--simulation", m_benchmarkBlock,
            "Mining test. Used to validate kernel optimizations. Specify block number", true);
    sim_opt->group(CommonGroup);

    app.add_option("--tstop", m_tstop,
            "Stop mining on a GPU if temperature exceeds value. 0 is disabled, valid: 30..100", true)
        ->group(CommonGroup)
        ->check(CLI::Range(30, 100));

    app.add_option("--tstart", m_tstart,
            "Restart mining on a GPU if the temperature drops below, valid: 30..100", true)
        ->group(CommonGroup)
        ->check(CLI::Range(30, 100));
    app.add_option("--coinbase-addr", m_coinbase_addr,
            "iCoinbase address")
        ->group(CommonGroup);


    std::stringstream ssHelp;
    ssHelp
        << "Pool URL Specification:" << endl
        << "    URL takes the form: scheme://user[:password]@hostname:port[/emailaddress]." << endl
        << "    for getwork use one of the following schemes:" << endl
        << "      " << URI::KnownSchemes(ProtocolFamily::GETWORK) << endl
        << "    for stratum use one of the following schemes: "<< endl
        << "      " << URI::KnownSchemes(ProtocolFamily::STRATUM) << endl
        << "    Stratum variants:" << endl
        << "    Example 1: stratum1+tcp://tPBQiizBs2tUGfLcM5pQeA6rYYCPyj6czL@<host>:<port/email" << endl
        << "    Example 2: stratum1+tcp://tPBQiizBs2tUGfLcM5pQeA6rYYCPyj6czL@<host>:<port>/<miner name>/email"
        << endl << endl;
    app.set_footer(ssHelp.str());

    try {
        app.parse(argc, argv);
        if (help) {
            std::cerr << endl << app.help() << endl;
            exit(0);
        } else if (vers) {
            auto* bi = energiminer_get_buildinfo();
            cerr << "\nenergiminer " << bi->project_version << "\nBuild: " << bi->system_name << "/"
                << bi->build_type << "/" << bi->compiler_id << "\n\n";
            exit(0);
        }
    } catch(const CLI::ParseError &e) {
        cerr << endl << e.what() << "\n\n";
        exit(-1);
    }

    if (hwmon_opt->count()) {
        m_show_hwmonitors = true;
        if (hwmon)
            m_show_power = true;
    }

    if (m_minerExecutionMode != MinerExecutionMode::kCPU) {
        if (!cl_miner && !cuda_miner && !mixed_miner && !bench_opt->count() && !sim_opt->count()) {
            cerr << endl << "One of -G, -U, -X, -M, or -Z must be specified" << "\n\n";
            exit(-1);
        }
    }

    if (cl_miner) {
        m_minerExecutionMode = MinerExecutionMode::kCL;
    } else if (cuda_miner) {
        m_minerExecutionMode = MinerExecutionMode::kCUDA;
    } else if (mixed_miner) {
        m_minerExecutionMode = MinerExecutionMode::kMixed;
    }
    if (bench_opt->count()) {
        m_mode = OperationMode::Benchmark;
    } else if (sim_opt->count()) {
        m_mode = OperationMode::Simulation;
    }
    for (auto url : pools) {
        if (url == "exit") // add fake scheme and port to 'exit' url
            url = "stratum+tcp://-:x@exit:0";
        URI uri(url);
        if (!uri.Valid()) {
            cerr << endl << "Bad endpoint address: " << url << "\n\n";
            exit(-1);
        }
        if (!uri.KnownScheme()) {
            cerr << endl << "Unknown URI scheme " << uri.Scheme() << "\n\n";
            exit(-1);
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
        //if ((m_mode != OperationMode::None) && (m_mode != mode)) {
        //    cerr << endl << "Mixed stratum and getwork endpoints not supported." << "\n\n";
        //    exit(-1);
        //}
        m_mode = mode;
    }

    if ((m_mode == OperationMode::None) && !m_shouldListDevices) {
        cerr << endl << "At least one pool URL must be specified" << "\n\n";
        exit(-1);
    }

#if ETH_ETHASHCL
    m_openclDeviceCount = m_openclDevices.size();
#endif

#if ETH_ETHASHCUDA
    m_cudaDeviceCount = m_cudaDevices.size();
    if (sched == "auto") {
        m_cudaSchedule = 0;
    } else if (sched == "spin") {
        m_cudaSchedule = 1;
    } else if (sched == "yield") {
        m_cudaSchedule = 2;
    } else if (sched == "sync") {
        m_cudaSchedule = 4;
    }
#endif

    if (m_tstop && (m_tstop <= m_tstart)) {
        cerr << endl << "tstop must be greater than tstart" << "\n\n";
        exit(-1);
    }
    if (m_tstop && !m_show_hwmonitors) {
        // if we want stop mining at a specific temperature, we have to
        // monitor the temperature ==> so auto enable HWMON.
        m_show_hwmonitors = true;
    }
}

void MinerCLI::execute()
{
    if (m_shouldListDevices) {
#if ETH_ETHASHCL
        if (m_minerExecutionMode == MinerExecutionMode::kCL ||
                m_minerExecutionMode == MinerExecutionMode::kMixed)
            OpenCLMiner::listDevices();
#endif
#if ETH_ETHASHCUDA
        if (m_minerExecutionMode == MinerExecutionMode::kCUDA ||
                m_minerExecutionMode == MinerExecutionMode::kMixed)
            CUDAMiner::listDevices();
#endif
        stop_io_service();
        return;
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
                    0,
                    m_dagLoadMode,
                    m_dagCreateDevice,
                    m_noEval,
                    m_exit))
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
        try {
            if (m_cudaDeviceCount > 0) {
                CUDAMiner::setDevices(m_cudaDevices, m_cudaDeviceCount);
                m_miningThreads = m_cudaDeviceCount;
            }

            CUDAMiner::setNumInstances(m_miningThreads);
        } catch (const std::runtime_error& err) {
            cwarn << "CUDA error: " << err.what();
            stop_io_service();
            exit(1);
        }
        if (!CUDAMiner::configureGPU(
                    m_cudaBlockSize,
                    m_cudaGridSize,
                    m_numStreams,
                    m_cudaSchedule,
                    m_dagLoadMode,
                    m_dagCreateDevice,
                    m_noEval,
                    m_exit
                    )) {
            stop_io_service();
            exit(1);
        }
        CUDAMiner::setParallelHash(m_cudaParallelHash);
#else
        cerr << "CUDA support disabled. Configure project build with -DETHASHCUDA=ON" << endl;
        stop_io_service();
        exit(1);
#endif
    }

    g_running = true;
    signal(SIGINT, MinerCLI::signalHandler);
    signal(SIGTERM, MinerCLI::signalHandler);

    switch (m_mode) {
        case OperationMode::Benchmark:
            //doBenchmark(m_minerExecutionMode, m_benchmarkWarmup, m_benchmarkTrial, m_benchmarkTrials);
            break;
        case OperationMode::GBT:
        case OperationMode::Stratum:
        case OperationMode::Simulation:
            doMiner();
            break;
        default:
            std::cerr << std::endl << "Program logic error" << "\n\n";
            std::exit(1);
    }
}

void MinerCLI::doMiner()
{
    PoolClient* client = nullptr;
    if (m_mode == OperationMode::GBT) {
			client = new GetworkClient(m_farmRecheckPeriod, m_coinbase_addr);
    } else if (m_mode == OperationMode::Stratum) {
        client = new StratumClient(m_io_service, m_worktimeout, m_responsetimeout, m_email, m_report_stratum_hashrate);
    } else if (m_mode == OperationMode::Simulation) {
        //client = new SimulationClient(20, m_benchmarkBlock);
        std::cout << "Selected simulation mode" << std::endl;
        return;
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
    PoolManager mgr(m_io_service, client, plant, m_minerExecutionMode, m_maxFarmRetries, m_failovertimeout);

    // If we are in simulation mode we add a fake connection
    if (m_mode == OperationMode::Simulation) {
        URI con(URI("http://-:0"));
        mgr.clearConnections();
        mgr.addConnection(con);
    } else {
        for (auto conn : m_endpoints) {
            cnote << "Configured pool " << conn.Host() + ":" + to_string(conn.Port());
            mgr.addConnection(conn);
        }
    }
    //start PoolManager
    mgr.start();

    unsigned interval = m_displayInterval;
    // Run CLI in loop
    while (g_running && mgr.isRunning()) {
        // Wait at the beginning of the loop to give some time
        // services to start properly. Otherwise we get a "not-connected"
        // message immediately
        this_thread::sleep_for(chrono::seconds(2));
        if (interval > 2) {
            interval -= 2;
            continue;
        }
        if (mgr.isConnected()) {
            auto mp = plant.miningProgress(m_show_hwmonitors, m_show_power);
            minelog << mp << ' ' << plant.getSolutionStats() << ' ' << plant.farmLaunchedFormatted();
        } else {
            minelog << "not-connected";
        }
        interval = m_displayInterval;
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
