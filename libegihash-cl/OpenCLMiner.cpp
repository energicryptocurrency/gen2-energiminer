/*
 * OpenCLMiner.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include "libegihash-cl/OpenCLMiner.h"

#include "../libegihash-cl/CL/cl2.hpp"
#include "energiminer/egihash/egihash.h"
#include "CLMiner_kernel.h"
#include "energiminer/common/Log.h"
#include "energiminer/CpuMiner.h"

#include <vector>
#include <iostream>

namespace {

const char* strClError(cl_int err)
{
    switch (err) {
        case CL_SUCCESS:
            return "CL_SUCCESS";
        case CL_DEVICE_NOT_FOUND:
            return "CL_DEVICE_NOT_FOUND";
        case CL_DEVICE_NOT_AVAILABLE:
            return "CL_DEVICE_NOT_AVAILABLE";
        case CL_COMPILER_NOT_AVAILABLE:
            return "CL_COMPILER_NOT_AVAILABLE";
        case CL_MEM_OBJECT_ALLOCATION_FAILURE:
            return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
        case CL_OUT_OF_RESOURCES:
            return "CL_OUT_OF_RESOURCES";
        case CL_OUT_OF_HOST_MEMORY:
            return "CL_OUT_OF_HOST_MEMORY";
        case CL_PROFILING_INFO_NOT_AVAILABLE:
            return "CL_PROFILING_INFO_NOT_AVAILABLE";
        case CL_MEM_COPY_OVERLAP:
            return "CL_MEM_COPY_OVERLAP";
        case CL_IMAGE_FORMAT_MISMATCH:
            return "CL_IMAGE_FORMAT_MISMATCH";
        case CL_IMAGE_FORMAT_NOT_SUPPORTED:
            return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
        case CL_BUILD_PROGRAM_FAILURE:
            return "CL_BUILD_PROGRAM_FAILURE";
        case CL_MAP_FAILURE:
            return "CL_MAP_FAILURE";
        case CL_MISALIGNED_SUB_BUFFER_OFFSET:
            return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
        case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:
            return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";

#ifdef CL_VERSION_1_2
        case CL_COMPILE_PROGRAM_FAILURE:
            return "CL_COMPILE_PROGRAM_FAILURE";
        case CL_LINKER_NOT_AVAILABLE:
            return "CL_LINKER_NOT_AVAILABLE";
        case CL_LINK_PROGRAM_FAILURE:
            return "CL_LINK_PROGRAM_FAILURE";
        case CL_DEVICE_PARTITION_FAILED:
            return "CL_DEVICE_PARTITION_FAILED";
        case CL_KERNEL_ARG_INFO_NOT_AVAILABLE:
            return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";
#endif // CL_VERSION_1_2

        case CL_INVALID_VALUE:
                return "CL_INVALID_VALUE";
        case CL_INVALID_DEVICE_TYPE:
            return "CL_INVALID_DEVICE_TYPE";
        case CL_INVALID_PLATFORM:
            return "CL_INVALID_PLATFORM";
        case CL_INVALID_DEVICE:
            return "CL_INVALID_DEVICE";
        case CL_INVALID_CONTEXT:
            return "CL_INVALID_CONTEXT";
        case CL_INVALID_QUEUE_PROPERTIES:
            return "CL_INVALID_QUEUE_PROPERTIES";
        case CL_INVALID_COMMAND_QUEUE:
            return "CL_INVALID_COMMAND_QUEUE";
        case CL_INVALID_HOST_PTR:
            return "CL_INVALID_HOST_PTR";
        case CL_INVALID_MEM_OBJECT:
            return "CL_INVALID_MEM_OBJECT";
        case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
            return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
        case CL_INVALID_IMAGE_SIZE:
            return "CL_INVALID_IMAGE_SIZE";
        case CL_INVALID_SAMPLER:
            return "CL_INVALID_SAMPLER";
        case CL_INVALID_BINARY:
            return "CL_INVALID_BINARY";
        case CL_INVALID_BUILD_OPTIONS:
            return "CL_INVALID_BUILD_OPTIONS";
        case CL_INVALID_PROGRAM:
            return "CL_INVALID_PROGRAM";
        case CL_INVALID_PROGRAM_EXECUTABLE:
            return "CL_INVALID_PROGRAM_EXECUTABLE";
        case CL_INVALID_KERNEL_NAME:
            return "CL_INVALID_KERNEL_NAME";
        case CL_INVALID_KERNEL_DEFINITION:
            return "CL_INVALID_KERNEL_DEFINITION";
        case CL_INVALID_KERNEL:
            return "CL_INVALID_KERNEL";
        case CL_INVALID_ARG_INDEX:
            return "CL_INVALID_ARG_INDEX";
        case CL_INVALID_ARG_VALUE:
            return "CL_INVALID_ARG_VALUE";
        case CL_INVALID_ARG_SIZE:
            return "CL_INVALID_ARG_SIZE";
        case CL_INVALID_KERNEL_ARGS:
            return "CL_INVALID_KERNEL_ARGS";
        case CL_INVALID_WORK_DIMENSION:
            return "CL_INVALID_WORK_DIMENSION";
        case CL_INVALID_WORK_GROUP_SIZE:
            return "CL_INVALID_WORK_GROUP_SIZE";
        case CL_INVALID_WORK_ITEM_SIZE:
                return "CL_INVALID_WORK_ITEM_SIZE";
        case CL_INVALID_GLOBAL_OFFSET:
            return "CL_INVALID_GLOBAL_OFFSET";
        case CL_INVALID_EVENT_WAIT_LIST:
            return "CL_INVALID_EVENT_WAIT_LIST";
        case CL_INVALID_EVENT:
            return "CL_INVALID_EVENT";
        case CL_INVALID_OPERATION:
            return "CL_INVALID_OPERATION";
        case CL_INVALID_GL_OBJECT:
            return "CL_INVALID_GL_OBJECT";
        case CL_INVALID_BUFFER_SIZE:
            return "CL_INVALID_BUFFER_SIZE";
        case CL_INVALID_MIP_LEVEL:
            return "CL_INVALID_MIP_LEVEL";
        case CL_INVALID_GLOBAL_WORK_SIZE:
            return "CL_INVALID_GLOBAL_WORK_SIZE";
        case CL_INVALID_PROPERTY:
            return "CL_INVALID_PROPERTY";

#ifdef CL_VERSION_1_2
        case CL_INVALID_IMAGE_DESCRIPTOR:
            return "CL_INVALID_IMAGE_DESCRIPTOR";
        case CL_INVALID_COMPILER_OPTIONS:
            return "CL_INVALID_COMPILER_OPTIONS";
        case CL_INVALID_LINKER_OPTIONS:
            return "CL_INVALID_LINKER_OPTIONS";
        case CL_INVALID_DEVICE_PARTITION_COUNT:
            return "CL_INVALID_DEVICE_PARTITION_COUNT";
#endif // CL_VERSION_1_2

#ifdef CL_VERSION_2_0
        case CL_INVALID_PIPE_SIZE:
            return "CL_INVALID_PIPE_SIZE";
        case CL_INVALID_DEVICE_QUEUE:
            return "CL_INVALID_DEVICE_QUEUE";
#endif // CL_VERSION_2_0

#ifdef CL_VERSION_2_2
        case CL_INVALID_SPEC_ID:
            return "CL_INVALID_SPEC_ID";
        case CL_MAX_SIZE_RESTRICTION_EXCEEDED:
            return "CL_MAX_SIZE_RESTRICTION_EXCEEDED";
#endif // CL_VERSION_2_2
    }
    return "Unknown CL error encountered";
}

std::string CLErrorHelper(const cl::Error& clerr)
{
    std::ostringstream osstream;
    osstream << ": " << clerr.what() << ": " << strClError(clerr.err())
        << " (" << clerr.err() << ")";
    return osstream.str();
}

inline void addDefinition(std::string& _source, char const* _id, unsigned _value)
{
    char buf[256];
    sprintf(buf, "#define %s %uu\n", _id, _value);
    _source.insert(_source.begin(), buf, buf + strlen(buf));
}

std::vector<cl::Platform> getPlatforms()
{
    std::vector<cl::Platform> platforms;
    try {
        cl::Platform::get(&platforms);
    } catch(cl::Error const& err) {
        throw err;
    }
    return platforms;
}

std::vector<cl::Device> getDevices(std::vector<cl::Platform> const& _platforms, unsigned _platformId)
{
    std::vector<cl::Device> devices;
    size_t platform_num = std::min<size_t>(_platformId, _platforms.size() - 1);
    try
    {
        _platforms[platform_num].getDevices( CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_ACCELERATOR, &devices );
    }
    catch (cl::Error const& err)
    {
        // if simply no devices found return empty vector
        if (err.err() != CL_DEVICE_NOT_FOUND)
            throw err;
    }
    return devices;
}


} //unnamed namespace

using namespace energi;

unsigned OpenCLMiner::s_workgroupSize = OpenCLMiner::c_defaultLocalWorkSize;
unsigned OpenCLMiner::s_initialGlobalWorkSize = OpenCLMiner::c_defaultGlobalWorkSizeMultiplier * OpenCLMiner::c_defaultLocalWorkSize;
unsigned OpenCLMiner::s_threadsPerHash = 8;
constexpr size_t c_maxSearchResults = 1;

unsigned OpenCLMiner::s_platformId = 0;
unsigned OpenCLMiner::s_numInstances = 0;
int OpenCLMiner::s_devices[16] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

struct CLChannel: public LogChannel
{
    static const char* name()
    {
        return EthOrange " cl";
    }

    static const int verbosity = 2;
    static const bool debug = false;
};

#define cllog clog(CLChannel)
#define ETHCL_LOG(_contents) cllog << _contents

struct OpenCLMiner::clInfo
{
    static std::tuple<bool, cl::Device, int, int, std::string> getDeviceInfo(int index);

    cl::Context             context_;
    cl::CommandQueue        queue_;
    cl::Kernel              kernelSearch_;
    cl::Kernel              kernelDag_;

    cl::Buffer              bufferDag_;
    cl::Buffer              bufferLight_;
    cl::Buffer              bufferHeader_;
    cl::Buffer              bufferTarget_;
    cl::Buffer              searchBuffer_;
};

OpenCLMiner::OpenCLMiner(const Plant& plant, unsigned index)
    : Miner("GPU/", plant, index)
    , cl(new clInfo)
{
}

OpenCLMiner::~OpenCLMiner()
{
    delete cl;
}

unsigned OpenCLMiner::getNumDevices()
{
    std::vector<cl::Platform> platforms = getPlatforms();
    if (platforms.empty())
        return 0;

    std::vector<cl::Device> devices = getDevices(platforms, s_platformId);
    if (devices.empty()) {
        return 0;
    }
    return devices.size();
}

void OpenCLMiner::listDevices()
{
    std::string outString ="\nListing OpenCL devices.\nFORMAT: [platformID] [deviceID] deviceName\n";
    unsigned int i = 0;

    std::vector<cl::Platform> platforms = getPlatforms();
    if (platforms.empty())
        return;


    for (unsigned j = 0; j < platforms.size(); ++j) {
        i = 0;
        std::vector<cl::Device> devices = getDevices(platforms, j);
        for (auto const& device: devices) {
            outString += "[" + std::to_string(j) + "] [" + std::to_string(i) + "] " + device.getInfo<CL_DEVICE_NAME>() + "\n";
            outString += "\tCL_DEVICE_TYPE: ";
            switch (device.getInfo<CL_DEVICE_TYPE>())
            {
                case CL_DEVICE_TYPE_CPU:
                    outString += "CPU\n";
                    break;
                case CL_DEVICE_TYPE_GPU:
                    outString += "GPU\n";
                    break;
                case CL_DEVICE_TYPE_ACCELERATOR:
                    outString += "ACCELERATOR\n";
                    break;
                default:
                    outString += "DEFAULT\n";
                    break;
            }
            outString += "\tCL_DEVICE_GLOBAL_MEM_SIZE: " + std::to_string(device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>()) + "\n";
            outString += "\tCL_DEVICE_MAX_MEM_ALLOC_SIZE: " + std::to_string(device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>()) + "\n";
            outString += "\tCL_DEVICE_MAX_WORK_GROUP_SIZE: " + std::to_string(device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>()) + "\n";
            ++i;
        }
    }

    cllog << outString;
}

bool OpenCLMiner::configureGPU(
        unsigned _localWorkSize,
        unsigned _globalWorkSizeMultiplier,
        unsigned _platformId,
        uint64_t _currentBlock
        )
{
    s_platformId = _platformId;
    _localWorkSize = ((_localWorkSize + 7) / 8) * 8;
    s_workgroupSize = _localWorkSize;
    s_initialGlobalWorkSize = _globalWorkSizeMultiplier * _localWorkSize;

    uint64_t dagSize = egihash::dag_t::get_full_size(_currentBlock);

    std::vector<cl::Platform> platforms = getPlatforms();
    if (platforms.empty())
        return false;

    if (_platformId >= platforms.size())
        return false;

    std::vector<cl::Device> devices = getDevices(platforms, _platformId);
    for (auto const& device: devices) {
        cl_ulong result = 0;
        device.getInfo(CL_DEVICE_GLOBAL_MEM_SIZE, &result);
        if (result >= dagSize) {
            cllog << "Found suitable OpenCL device ["
                  << device.getInfo<CL_DEVICE_NAME>()
                  << "] with " << result << " bytes of GPU memory";
            return true;
        }
        cwarn << "OpenCL device " << device.getInfo<CL_DEVICE_NAME>() << " has insufficient GPU memory." << result <<
            " bytes of memory found < " << dagSize << " bytes of memory required";
    }
    cllog << "No GPU device with sufficient memory was found. Can't GPU mine. Remove the -G argument" ;
    return false;
}


void OpenCLMiner::trun()
{
    // Memory for zero-ing buffers. Cannot be static because crashes on macOS.
    uint32_t const c_zero = 0;
    uint32_t startNonce = 0;
    Work current_work; // Here we need current work as to initialize gpu
    try {
        while (true) {
            Work work = this->work(); // This work is a copy of last assigned work the worker was provided by plant
            if ( !work.isValid() ) {
                cdebug << "No work received. Pause for 1 s.";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if ( this->shouldStop() ) {
                    break;
                }
                continue;
            } else {
                //cnote << "Valid work.";
            }
            if ( current_work != work ) {
                cllog << "Bits:" << work.bitsNum << " " << work.nBits;
                auto localSwitchStart = std::chrono::high_resolution_clock::now();

                if (!dagLoaded_) {
                    init_dag();
                    dagLoaded_ = true;
                }
                energi::CBlockHeaderTruncatedLE truncatedBlockHeader(work);
                egihash::h256_t hash_header(&truncatedBlockHeader, sizeof(truncatedBlockHeader));

                // Update header constant buffer.
                cl->queue_.enqueueWriteBuffer(cl->bufferHeader_, CL_FALSE, 0, sizeof(hash_header), hash_header.b);
                cl->queue_.enqueueWriteBuffer(cl->searchBuffer_, CL_FALSE, 0, sizeof(c_zero), &c_zero);
                cllog << "Target Buffer ..." << sizeof(work.hashTarget);
                cl->queue_.enqueueWriteBuffer(cl->bufferTarget_, CL_FALSE, 0, sizeof(work.hashTarget), work.hashTarget.ToString().c_str());
                cllog << "Loaded";

                cl->kernelSearch_.setArg(0, cl->searchBuffer_);  // Supply output buffer to kernel.
                cl->kernelSearch_.setArg(4, cl->bufferTarget_);

                startNonce  = get_start_nonce();
                cllog << "Nonce loaded" << startNonce;

                auto switchEnd = std::chrono::high_resolution_clock::now();
                auto globalSwitchTime = std::chrono::duration_cast<std::chrono::milliseconds>(switchEnd - workSwitchStart).count();
                auto localSwitchTime = std::chrono::duration_cast<std::chrono::microseconds>(switchEnd - localSwitchStart).count();
                cllog << "Switch time" << globalSwitchTime << "ms /" << localSwitchTime << "us";
            }
            // Read results.
            // TODO: could use pinned host pointer instead.
            uint32_t results[c_maxSearchResults + 1];
            cl->queue_.enqueueReadBuffer(cl->searchBuffer_, CL_TRUE, 0, sizeof(results), &results);
            //cllog << "results[0]: " << results[0] << " [1]: " << results[1];

            uint32_t nonce = 0;
            if (results[0] > 0) {
                // Ignore results except the first one.
                nonce = startNonce + results[1];
                // Reset search buffer if any solution found.
                cl->queue_.enqueueWriteBuffer(cl->searchBuffer_, CL_FALSE, 0, sizeof(c_zero), &c_zero);
            }
            // Run the kernel.
            cl->kernelSearch_.setArg(3, startNonce);
            cl->queue_.enqueueNDRangeKernel(cl->kernelSearch_, cl::NullRange, globalWorkSize_, workgroupSize_);
            // Report results while the kernel is running.
            // It takes some time because ethash must be re-evaluated on CPU.
            if (nonce != 0) {
                work.nNonce = nonce;
                GetPOWHash(work);
                addHashCount(globalWorkSize_);
                Solution solution(work, nonce, work.hashMix);
                plant_.submit(solution);
            }
            current_work = work;

            addHashCount(globalWorkSize_);
            // Increase start nonce for following kernel execution.
            startNonce += globalWorkSize_;
            // Check if we should stop.
            if (shouldStop()) {
                // Make sure the last buffer write has finished --
                // it reads local variable.
                cl->queue_.finish();
                break;
            }
        }
    } catch (cl::Error const& _e) {
        cwarn << "OpenCL Error:" << CLErrorHelper(_e);
    }
}


std::tuple<bool, cl::Device, int, int, std::string> OpenCLMiner::clInfo::getDeviceInfo(int index)
{
    auto failResult = std::make_tuple(false, cl::Device(), 0, 0, "");
    std::vector<cl::Platform> platforms = getPlatforms();
    if (platforms.empty()) {
        return failResult;
    }

    // use selected platform
    unsigned platformIdx = std::min<unsigned>(s_platformId, platforms.size() - 1);
    std::string platformName = platforms[platformIdx].getInfo<CL_PLATFORM_NAME>();
    ETHCL_LOG("Platform: " << platformName);

    int platformId = OPENCL_PLATFORM_UNKNOWN;
    if (platformName == "NVIDIA CUDA") {
        platformId = OPENCL_PLATFORM_NVIDIA;
    } else if (platformName == "AMD Accelerated Parallel Processing") {
        platformId = OPENCL_PLATFORM_AMD;
    } else if (platformName == "Clover") {
        platformId = OPENCL_PLATFORM_CLOVER;
    }

    // get GPU device of the default platform
    std::vector<cl::Device> devices = getDevices(platforms, platformIdx);
    if (devices.empty()) {
        ETHCL_LOG("No OpenCL devices found.");
        return failResult;
    }

    // use selected device
    unsigned deviceId = s_devices[index] > -1 ? s_devices[index] : index;
    cl::Device& device = devices[std::min<unsigned>(deviceId, devices.size() - 1)];
    std::string device_version = device.getInfo<CL_DEVICE_VERSION>();
    ETHCL_LOG("Device:   " << device.getInfo<CL_DEVICE_NAME>() << " / " << device_version);

    std::string clVer = device_version.substr(7, 3);

    if (clVer == "1.0" || clVer == "1.1") {
        if (platformId == OPENCL_PLATFORM_CLOVER) {
            ETHCL_LOG("OpenCL " << clVer << " not supported, but platform Clover might work nevertheless. USE AT OWN RISK!");
        } else {
            ETHCL_LOG("OpenCL " << clVer << " not supported - minimum required version is 1.2");
            return failResult;
        }
    }

    char options[256];
    int computeCapability = 0;
    if (platformId == OPENCL_PLATFORM_NVIDIA) {
        cl_uint computeCapabilityMajor;
        cl_uint computeCapabilityMinor;
        clGetDeviceInfo(device(), CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV, sizeof(cl_uint), &computeCapabilityMajor, NULL);
        clGetDeviceInfo(device(), CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV, sizeof(cl_uint), &computeCapabilityMinor, NULL);

        computeCapability = computeCapabilityMajor * 10 + computeCapabilityMinor;
        int maxregs = computeCapability >= 35 ? 72 : 63;
        sprintf(options, "-cl-nv-maxrregcount=%d", maxregs);
    } else {
        sprintf(options, "%s", "");
    }

    return std::make_tuple(true, device, platformId, computeCapability, std::string(options));
}

bool OpenCLMiner::init_dag()
{
    const auto& dag = ActiveDAG();
    // get all platforms
    try {
        auto deviceResult = cl->getDeviceInfo(index_);
        // create context
        auto device = std::get<1>(deviceResult);
        cl->context_  = cl::Context(std::vector<cl::Device>(&device, &device + 1));
        cl->queue_    = cl::CommandQueue(cl->context_, device);

        // make sure that global work size is evenly divisible by the local workgroup size
        workgroupSize_        = s_workgroupSize;
        globalWorkSize_       = s_initialGlobalWorkSize;
        if (globalWorkSize_ % workgroupSize_ != 0) {
            globalWorkSize_ = ((globalWorkSize_ / workgroupSize_) + 1) * workgroupSize_;
        }
        uint64_t dagSize = dag->size();
        uint32_t dagSize128 = (unsigned)(dagSize / egihash::constants::MIX_BYTES);
        uint32_t lightSize64 = dag->get_cache().data().size();
        // patch source code
        // note: CLMiner_kernel is simply ethash_cl_miner_kernel.cl compiled
        // into a byte array by bin2h.cmake. There is no need to load the file by hand in runtime
        // TODO: Just use C++ raw string literal.
        std::string code(CLMiner_kernel, CLMiner_kernel + sizeof(CLMiner_kernel));

        addDefinition(code, "GROUP_SIZE", workgroupSize_);
        addDefinition(code, "DAG_SIZE", dagSize128);
        addDefinition(code, "LIGHT_SIZE", lightSize64);
        addDefinition(code, "ACCESSES", egihash::constants::ACCESSES);
        addDefinition(code, "MAX_OUTPUTS", c_maxSearchResults);
        addDefinition(code, "PLATFORM", std::get<2>(deviceResult));
        addDefinition(code, "COMPUTE", std::get<3>(deviceResult));
        addDefinition(code, "THREADS_PER_HASH", s_threadsPerHash);

        cnote << "DAG GROUP_SIZE=" << workgroupSize_;
        cnote << "DAG_SIZE=" << dagSize;
        cnote << "DAG_SIZE(128)=" << dagSize128;
        cnote << "LIGHT_SIZE=" << lightSize64;
        cnote << "ACCESSES=" << egihash::constants::ACCESSES;
        cnote << "MAX_OUTPUTS=" << c_maxSearchResults;
        cnote << "PLATTFORM=" << std::get<2>(deviceResult);
        cnote << "COMPUTE=" << std::get<3>(deviceResult);
        cnote << "THREADS_PER_HASH=" << s_threadsPerHash;

        // create miner OpenCL program
        cl::Program::Sources sources{{code.data(), code.size()}};
        cl::Program program(cl->context_, sources);
        try {
            program.build({device}, std::get<4>(deviceResult).c_str());
            cllog << "Build info:" << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
        } catch (cl::Error const&) {
            cwarn << "Build info:" << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
            cwarn << " Failed" ;
            return false;
        }

        // create buffer for dag
        std::vector<uint32_t> vData;
        for (auto &d : dag->get_cache().data()) {
            for ( auto &dv : d) {
                vData.push_back(dv.hword);
            }
        }

        cl_ulong result = 0;
        device.getInfo(CL_DEVICE_GLOBAL_MEM_SIZE, &result);
        if (result < dagSize) {
            cnote << "OpenCL device " << device.getInfo<CL_DEVICE_NAME>()
                << " has insufficient GPU memory." << result
                << " bytes of memory found < " << dagSize << " bytes of memory required";
            return false;
        }
        try {
            cl->bufferLight_      = cl::Buffer(cl->context_, CL_MEM_READ_ONLY, sizeof(uint32_t) * vData.size());
            cl->bufferDag_        = cl::Buffer(cl->context_, CL_MEM_READ_ONLY, dagSize);

            cl->kernelSearch_     = cl::Kernel(program, "ethash_search");
            cl->kernelDag_        = cl::Kernel(program, "ethash_calculate_dag_item");

            ETHCL_LOG("Creating light buffer");

            cl->queue_.enqueueWriteBuffer(cl->bufferLight_, CL_TRUE, 0, sizeof(uint32_t) * vData.size(), vData.data());
        } catch (cl::Error const& err) {
            cwarn << "Creating DAG buffer failed:" << err.what() << err.err();
            return false;
        }

        // create buffer for header
        ETHCL_LOG("Creating buffer for header.");
        cl->bufferHeader_ = cl::Buffer(cl->context_, CL_MEM_READ_ONLY, 32);

        cl->kernelSearch_.setArg(1, cl->bufferHeader_);
        cl->kernelSearch_.setArg(2, cl->bufferDag_);
        cl->kernelSearch_.setArg(5, ~0u);  // Pass this to stop the compiler unrolling the loops.

        // create mining buffers
        ETHCL_LOG("Creating mining buffer");
        cl->searchBuffer_ = cl::Buffer(cl->context_, CL_MEM_WRITE_ONLY, (c_maxSearchResults + 1) * sizeof(uint32_t));
        cl->bufferTarget_ = cl::Buffer(cl->context_, CL_MEM_READ_ONLY, 32);

        cllog << "Generating DAG";

        uint32_t const work = (uint32_t)(dagSize / sizeof(egihash::node));
        uint32_t fullRuns = work / globalWorkSize_;
        uint32_t const restWork = work % globalWorkSize_;
        if (restWork > 0) fullRuns++;

        cl->kernelDag_.setArg(1, cl->bufferLight_);
        cl->kernelDag_.setArg(2, cl->bufferDag_);
        cl->kernelDag_.setArg(3, ~0u);

        for (uint32_t i = 0; i < fullRuns; ++i) {
            cl->kernelDag_.setArg(0, i * globalWorkSize_);
            cl->queue_.enqueueNDRangeKernel(cl->kernelDag_, cl::NullRange, globalWorkSize_, workgroupSize_);
            cl->queue_.finish();
        }

        cllog << "DAG Loaded" ;

        std::this_thread::sleep_for(std::chrono::seconds(10));
    } catch (cl::Error const& err) {
        cwarn << err.what() << "(" << err.err() << ")";
        return false;
    }
    return true;
}
