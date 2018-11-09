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

#include "CUDAMiner.h"
#include "nrghash/nrghash.h"
#include "common/Log.h"

#include <algorithm>
#include <iostream>

using namespace std;
using namespace energi;

unsigned CUDAMiner::s_numInstances = 0;

std::vector<int> CUDAMiner::s_devices(MAX_MINERS, -1);

struct CUDAChannel: public LogChannel
{
    static const char* name() { return EthOrange "cu"; }
    static const int verbosity = 2;
    static const bool debug = false;
};

#define cudalog clog(CUDAChannel)

CUDAMiner::CUDAMiner(const Plant& plant, unsigned index)
    : Miner("GPU/", plant, index)
    , m_light(getNumDevices())
{
}

CUDAMiner::~CUDAMiner()
{
    stopWorking();
}

bool CUDAMiner::init_dag(uint32_t height)
{
    //!@brief get all platforms
    try {
        uint32_t epoch = height / nrghash::constants::EPOCH_LENGTH;
        cudalog << name() << " Generating DAG for epoch #" << epoch;
        if (s_dagLoadMode == DAG_LOAD_MODE_SEQUENTIAL)
            while (s_dagLoadIndex < m_index)
                this_thread::sleep_for(chrono::milliseconds(100));
        unsigned device = s_devices[m_index] > -1 ? s_devices[m_index] : m_index;

        cnote << "Initialising miner " << m_index;

        cuda_init(getNumDevices(), height, device, (s_dagLoadMode == DAG_LOAD_MODE_SINGLE),
            s_dagInHostMemory, s_dagCreateDevice);
        s_dagLoadIndex++;

        if (s_dagLoadMode == DAG_LOAD_MODE_SINGLE) {
            if (s_dagLoadIndex >= s_numInstances && s_dagInHostMemory) {
                // all devices have loaded DAG, we can free now
                delete[] s_dagInHostMemory;
                s_dagInHostMemory = nullptr;
                cnote << "Freeing DAG from host";
            }
        }
        return true;
    } catch (std::runtime_error const& _e) {
        cwarn << "Error CUDA mining: " << _e.what();
        if(s_exit) {
            exit(1);
        }
        return false;
    }
}

void CUDAMiner::trun()
{
    setThreadName("CUDA");
    try {
        while(!shouldStop()) {
            if (is_mining_paused()) {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                continue;
            }

            if (haveNewWork()) {
                m_current = getWork();

                if ( !m_current.isValid() ) {
                    continue;
                }

                auto height = m_current.nHeight;
                auto new_epoch = height / nrghash::constants::EPOCH_LENGTH;
                auto last_epoch = m_lastHeight / nrghash::constants::EPOCH_LENGTH;

                if (!m_dagLoaded || (new_epoch != last_epoch)) {
                    init_dag(height);
                    cnote << "End initialising";
                    m_dagLoaded = true;
                }

                m_lastHeight = height;
            }

            if(!m_current.isValid()) {
                waitMoreWork();
                continue;
            }

            energi::CBlockHeaderTruncatedLE truncatedBlockHeader(m_current);
            nrghash::h256_t hash_header(&truncatedBlockHeader, sizeof(truncatedBlockHeader));

            // Upper 64 bits of the boundary.
            const uint64_t upper64OfBoundary = *reinterpret_cast<uint64_t const *>((m_current.hashTarget >> 192).data());
            assert(upper64OfBoundary > 0);
            uint64_t startN = m_plant.getStartNonce(m_current, m_index);

            search(hash_header.data(), upper64OfBoundary, startN, m_current);
        }
        // Reset miner and stop working
        CUDA_SAFE_CALL(cudaDeviceReset());
    } catch (cuda_runtime_error const& _e) {
        cwarn << "GPU error: " << _e.what();
        if(s_exit) {
            cwarn << "Terminating.";
            exit(1);
        }
    }
}

void CUDAMiner::setNumInstances(unsigned _instances)
{
    s_numInstances = std::min<unsigned>(_instances, getNumDevices());
}

void CUDAMiner::setDevices(const std::vector<unsigned>& _devices, unsigned _selectedDeviceCount)
{
    for (unsigned i = 0; i < _selectedDeviceCount; ++i) {
        s_devices[i] = _devices[i];
    }
}

unsigned CUDAMiner::getNumDevices()
{
    int deviceCount;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    if (err == cudaSuccess) {
        return deviceCount;
    }

    if (err == cudaErrorInsufficientDriver) {
        int driverVersion = 0;
        cudaDriverGetVersion(&driverVersion);
        if (driverVersion == 0) {
            throw std::runtime_error{"No CUDA driver found"};
        }
        throw std::runtime_error{"Insufficient CUDA driver: " + std::to_string(driverVersion)};
    }
    throw std::runtime_error{cudaGetErrorString(err)};
}

void CUDAMiner::listDevices()
{
    try {
        cout << "\nListing CUDA devices.\nFORMAT: [deviceID] deviceName\n";
        int numDevices = getNumDevices();
        for (int i = 0; i < numDevices; ++i) {
            cudaDeviceProp props;
            CUDA_SAFE_CALL(cudaGetDeviceProperties(&props, i));

            cout << "[" + to_string(i) + "] " + string(props.name) + "\n";
            cout << "\tCompute version: " + to_string(props.major) + "." + to_string(props.minor) + "\n";
            cout << "\tcudaDeviceProp::totalGlobalMem: " + to_string(props.totalGlobalMem) + "\n";
            cout << "\tPci: " << setw(4) << setfill('0') << hex << props.pciDomainID << ':' << setw(2)
                << props.pciBusID << ':' << setw(2) << props.pciDeviceID << '\n';
        }
    } catch(const std::runtime_error& err) {
        cwarn << "CUDA error: " << err.what();
        if(s_exit) {
            exit(1);
        }
    }
}

bool CUDAMiner::configureGPU(
    unsigned _blockSize,
    unsigned _gridSize,
    unsigned _numStreams,
    unsigned _scheduleFlag,
    unsigned _dagLoadMode,
    unsigned _dagCreateDevice,
    bool _noeval,
    bool _exit
    )
{
    s_dagLoadMode = _dagLoadMode;
    s_dagCreateDevice = _dagCreateDevice;
    s_exit  = _exit;

    try {
        if (!cuda_configureGPU(
                    getNumDevices(),
                    s_devices,
                    ((_blockSize + 7) / 8) * 8,
                    _gridSize,
                    _numStreams,
                    _scheduleFlag,
                    _noeval)
           )
        {
            cout << "No CUDA device with sufficient memory was found. Can't CUDA mine. Remove the -U argument" << endl;
            return false;
        }
        return true;
    } catch (const std::exception& ex) {
        std::cerr << ex.what();
        std::exit(0);
    }
}

void CUDAMiner::setParallelHash(unsigned _parallelHash)
{
      m_parallelHash = _parallelHash;
}

unsigned const CUDAMiner::c_defaultBlockSize = 128;
unsigned const CUDAMiner::c_defaultGridSize = 8192; // * CL_DEFAULT_LOCAL_WORK_SIZE
unsigned const CUDAMiner::c_defaultNumStreams = 2;

bool CUDAMiner::cuda_configureGPU(
    size_t numDevices,
    const vector<int>& _devices,
    unsigned _blockSize,
    unsigned _gridSize,
    unsigned _numStreams,
    unsigned _scheduleFlag,
    bool _noeval
    )
{
    try {
        s_blockSize = _blockSize;
        s_gridSize = _gridSize;
        s_numStreams = _numStreams;
        s_scheduleFlag = _scheduleFlag;
        s_noeval = _noeval;

        cudalog << "Using grid size " << s_gridSize << ", block size " << s_blockSize;

        // by default let's only consider the DAG of the first epoch
         uint64_t dagSize = nrghash::dag_t::get_full_size(0);
        int devicesCount = static_cast<int>(numDevices);
        for (int i = 0; i < devicesCount; ++i) {
            if (_devices[i] != -1) {
                int deviceId = min(devicesCount - 1, _devices[i]);
                cudaDeviceProp props;
                CUDA_SAFE_CALL(cudaGetDeviceProperties(&props, deviceId));
                if (props.totalGlobalMem >= dagSize) {
                    cudalog <<  "Found suitable CUDA device [" << string(props.name) << "] with " << props.totalGlobalMem << " bytes of GPU memory";
                } else {
                    cudalog <<  "CUDA device " << string(props.name) << " has insufficient GPU memory."
                            << FormattedMemSize(props.totalGlobalMem) << " of memory found < "
                            << FormattedMemSize(dagSize) << " of memory required";
                    return false;
                }
            }
        }
        return true;
    } catch (runtime_error) {
        if(s_exit) {
            exit(1);
        }
        return false;
    }
}

unsigned CUDAMiner::m_parallelHash = 4;
unsigned CUDAMiner::s_blockSize = CUDAMiner::c_defaultBlockSize;
unsigned CUDAMiner::s_gridSize = CUDAMiner::c_defaultGridSize;
unsigned CUDAMiner::s_numStreams = CUDAMiner::c_defaultNumStreams;
unsigned CUDAMiner::s_scheduleFlag = 0;

bool CUDAMiner::cuda_init(
    size_t numDevices,
    uint32_t height,
    unsigned _deviceId,
    bool _cpyToHost,
    uint8_t* &hostDAG,
    unsigned dagCreateDevice)
{
    try {
        if (numDevices == 0) {
            return false;
        }
        // use selected device
        m_device_num = _deviceId < numDevices -1 ? _deviceId : numDevices - 1;
        m_hwmoninfo.deviceType = HwMonitorInfoType::NVIDIA;
        m_hwmoninfo.indexSource = HwMonitorIndexSource::CUDA;
        m_hwmoninfo.deviceIndex = m_device_num;

        cudaDeviceProp device_props;
        CUDA_SAFE_CALL(cudaGetDeviceProperties(&device_props, m_device_num));

        cudalog << "Using device: " << device_props.name << " (Compute " + to_string(device_props.major) + "." + to_string(device_props.minor) + ")";

        m_search_buf = new volatile Search_results * [s_numStreams];
        m_streams = new cudaStream_t[s_numStreams];

        nrghash::cache_t cache = nrghash::cache_t(height);
        uint64_t dagSize = nrghash::dag_t::get_full_size(height);
        const auto lightNumItems = (unsigned)(cache.data().size());
        const auto dagNumItems = (unsigned)(dagSize / nrghash::constants::MIX_BYTES);
        // create buffer for dag
        std::vector<uint32_t> vData;
        for (auto &d : cache.data()) {
            for ( auto &dv : d) {
                vData.push_back(dv.hword);
            }
        }
        const auto lightSize = sizeof(uint32_t) * vData.size();

        CUDA_SAFE_CALL(cudaSetDevice(m_device_num));
        cudalog << "Set Device to current";
        if (dagNumItems != m_dag_size || !m_dag) {
            //Check whether the current device has sufficient memory every time we recreate the dag
            if (device_props.totalGlobalMem < dagSize) {
                cudalog <<  "CUDA device " << string(device_props.name)
                        << " has insufficient GPU memory." << FormattedMemSize(device_props.totalGlobalMem) << " of memory found < "
                        << FormattedMemSize(dagSize) << " of memory required";
                return false;
            }
            //We need to reset the device and recreate the dag
            cudalog << "Resetting device";
            CUDA_SAFE_CALL(cudaDeviceReset());
            CUDA_SAFE_CALL(cudaSetDeviceFlags(s_scheduleFlag));
            CUDA_SAFE_CALL(cudaDeviceSetCacheConfig(cudaFuncCachePreferL1));
            //We need to reset the light and the Dag for the following code to reallocate
            //since cudaDeviceReset() frees all previous allocated memory
            m_light[m_device_num] = nullptr;
            m_dag = nullptr;
        }
        // create buffer for cache
        hash128_t* dag = m_dag;
        hash64_t* light = m_light[m_device_num];

        if(!light) {
            cudalog << "Allocating light with size: " << FormattedMemSize(lightSize);
            CUDA_SAFE_CALL(cudaMalloc(reinterpret_cast<void**>(&light), lightSize));
        }
        // copy lightData to device
        CUDA_SAFE_CALL(cudaMemcpy(light, vData.data(), lightSize, cudaMemcpyHostToDevice));
        m_light[m_device_num] = light;

        if (dagNumItems != m_dag_size || !dag) { // create buffer for dag
            CUDA_SAFE_CALL(cudaMalloc(reinterpret_cast<void**>(&dag), dagSize));
        }

        set_constants(dag, dagNumItems, light, lightNumItems); //in ethash_cuda_miner_kernel.cu

        if (dagNumItems != m_dag_size || !dag) {
            // create mining buffers
            cudalog << "Generating mining buffers";
            for (unsigned i = 0; i != s_numStreams; ++i) {
                CUDA_SAFE_CALL(cudaMallocHost(&m_search_buf[i], sizeof(Search_results)));
                CUDA_SAFE_CALL(cudaStreamCreateWithFlags(&m_streams[i], cudaStreamNonBlocking));
            }
            m_current_target = 0;
            if (!hostDAG) {
                if((m_device_num == dagCreateDevice) || !_cpyToHost) { //if !cpyToHost -> All devices shall generate their DAG
                    cudalog << "Generating DAG for GPU #" << m_device_num
                        << " with dagSize: " << FormattedMemSize(dagSize) << " ("
                        << FormattedMemSize(device_props.totalGlobalMem - dagSize - lightSize) << " left)";
                    auto startDAG = std::chrono::steady_clock::now();

                    ethash_generate_dag(dagSize, s_gridSize, s_blockSize, m_streams[0]);
                    cudalog << "Generated DAG for GPU #" << m_device_num << " in: "
                            << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startDAG).count()
                            << " ms.";

                    if (_cpyToHost) {
                        uint8_t* memoryDAG = new uint8_t[dagSize];
                        cudalog << "Copying DAG from GPU #" << m_device_num << " to host";
                        CUDA_SAFE_CALL(cudaMemcpy(reinterpret_cast<void*>(memoryDAG), dag, dagSize, cudaMemcpyDeviceToHost));

                        hostDAG = memoryDAG;
                    }
                } else {
                    while(!hostDAG) {
                        this_thread::sleep_for(chrono::milliseconds(100));
                    }
                    goto cpyDag;
                }
            } else {
cpyDag:
                cudalog << "Copying DAG from host to GPU #" << m_device_num;
                const void* hdag = (const void*)hostDAG;
                CUDA_SAFE_CALL(cudaMemcpy(reinterpret_cast<void*>(dag), hdag, dagSize, cudaMemcpyHostToDevice));
            }
        }

        m_dag = dag;
        m_dag_size = dagNumItems;
        return true;
    } catch (const runtime_error&) {
        if(s_exit) {
            exit(1);
        }
        return false;
    }
}

void CUDAMiner::search(
    uint8_t const* header,
    uint64_t target,
    uint64_t startN,
    Work& work)
{
    const uint16_t kReportingInterval = 4;  // Must be a power of 2 passes
    set_header(*reinterpret_cast<hash32_t const *>(header));
    if (m_current_target != target) {
        set_target(target);
        m_current_target = target;
    }

    // choose the starting nonce
    uint64_t current_nonce = startN;

    // Nonces processed in one pass by a single stream
    const uint32_t batch_size = s_gridSize * s_blockSize;
    // Nonces processed in one pass by all streams
    const uint32_t streams_batch_size = batch_size * s_numStreams;

    // prime each stream and clear search result buffers
    uint32_t current_index;
    for (current_index = 0; current_index < s_numStreams; current_index++, current_nonce += batch_size) {
        cudaStream_t stream = m_streams[current_index];
        auto buffer = m_search_buf[current_index];
        buffer->count = 0;
        run_ethash_search(s_gridSize, s_blockSize, stream, buffer, current_nonce, m_parallelHash);
    }

    // process stream batches until we get new work.
    while (!haveNewWork() && !shouldStop()) {
        for (current_index = 0; current_index < s_numStreams; current_index++, current_nonce += batch_size) {
            cudaStream_t stream = m_streams[current_index];
            auto buffer = m_search_buf[current_index];
            // Wait for stream batch to complete and immediately
            // store number of processed hashes
            CUDA_SAFE_CALL(cudaStreamSynchronize(stream));
            // stretch cuda passes to miniize the effects of
            // OS latency variability
            m_searchPasses++;
            if ((m_searchPasses & (kReportingInterval - 1)) == 0)
                updateHashRate(batch_size * kReportingInterval);

            // See if we got solutions in this batch
            auto found_count = std::min((unsigned)buffer->count, MAX_SEARCH_RESULTS);
            auto results = buffer->result;
            buffer->count = 0;

            // restart ASAP
            run_ethash_search(s_gridSize, s_blockSize, stream, buffer, current_nonce, m_parallelHash);
            
            if (found_count) {
                uint64_t nonce_base = current_nonce - streams_batch_size;

                // Pass the solution(s) for submission
                for (uint32_t i = 0; (i < found_count) && !haveNewWork(); i++) {
                    work.nNonce = nonce_base + results[i].gid;
                    
                    auto const powHash = GetPOWHash(work);
                    
                    if (UintToArith256(powHash) <= work.hashTarget) {
                        cudalog << name()
                                << " Submitting block blockhash: "
                                << work.GetHash().ToString()
                                << " height: " << work.nHeight
                                << " nonce: " << work.nNonce;
                        m_plant.submitProof(work);
                    } else {
                        cwarn << name()
                            << " CUDA Miner proposed invalid solution: "
                            << work.GetHash().ToString()
                            << " nonce: " << work.nNonce;
                    }
                }

                if (found_count == MAX_SEARCH_RESULTS) {
                    cwarn << name()
                        << " CUDA result limit hit - report to developers!";
                }
            }
        }
    }

    if (g_logVerbosity >= 6) {
        cudalog << "Switch time: "
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - workSwitchStart)
                       .count()
                << " ms.";
    }
}

