/*
 * OpenCLMiner.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include "libegihash-cl/OpenCLMiner.h"

#include "energiminer/egihash/egihash.h"
#include "CLMiner_kernel.h"
#include "energiminer/Log.h"
#include "energiminer/arith_uint256.h"
#include "energiminer/CpuMiner.h"

#include <vector>
#include <iostream>

namespace energi
{
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



  inline void addDefinition(std::string& _source, char const* _id, unsigned _value)
  {
    char buf[256];
    sprintf(buf, "#define %s %uu\n", _id, _value);
    _source.insert(_source.begin(), buf, buf + strlen(buf));
  }

  std::vector<cl::Platform> getPlatforms()
  {
    std::vector<cl::Platform> platforms;
    try
    {
      cl::Platform::get(&platforms);
    }
    catch(cl::Error const& err)
    {
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

  unsigned OpenCLMiner::getNumDevices()
  {
    std::vector<cl::Platform> platforms = getPlatforms();
    if (platforms.empty())
      return 0;

    std::vector<cl::Device> devices = getDevices(platforms, s_platformId);
    if (devices.empty())
    {
      //cwarn << "No OpenCL devices found.";
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


    for (unsigned j = 0; j < platforms.size(); ++j)
    {
      i = 0;
      std::vector<cl::Device> devices = getDevices(platforms, j);
      for (auto const& device: devices)
      {
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
    for (auto const& device: devices)
    {
      cl_ulong result = 0;
      device.getInfo(CL_DEVICE_GLOBAL_MEM_SIZE, &result);
      if (result >= dagSize)
      {
        cllog << "Found suitable OpenCL device [" << device.getInfo<CL_DEVICE_NAME>() << "] with " << result << " bytes of GPU memory";
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
    uint64_t startNonce = 0;
    Work current_work; // Here we need current work as to initialize gpu
    try
    {
      bool dagLoaded = false;
      while (true)
      {
        Work work = this->work(); // This work is a copy of last assigned work the worker was provided by plant
        if ( !work.isValid() )
        {
          cdebug << "No work received. Pause for 1 s.";
          std::this_thread::sleep_for(std::chrono::seconds(1));
          continue;
        }

        if ( current_work != work )
        {
          uint32_t _ALIGN(128) endiandata[21];
          uint32_t *pdata     = work.blockHeader.data();

          for (int k=0; k < 20; k++) // we dont use mixHash part to calculate hash but fill it later (below)
          {
            be32enc(&endiandata[k], pdata[k]);
          }

          auto hash_header = CpuMiner::GetHeaderHash(endiandata);
          cllog << "Bits:" << work.bitsNum << " " << work.bits;
          arith_uint256 hashTarget = arith_uint256(work.bits);
          cllog << "Target Compact:" << hashTarget.GetCompact();
          cllog << "Target Low64:" << hashTarget.GetLow64();
          auto localSwitchStart = std::chrono::high_resolution_clock::now();

          if (!dagLoaded || ( egihash::get_seedhash(current_work.height) != egihash::get_seedhash(work.height) ) )
          {
            init_dag(egihash::get_seedhash(work.height), work.height);
            dagLoaded = true;
          }

          current_work = work;

          // Update header constant buffer.
          queue_.enqueueWriteBuffer(bufferHeader_, CL_FALSE, 0, sizeof(hash_header), hash_header.b);
          queue_.enqueueWriteBuffer(bufferSearch_, CL_FALSE, 0, sizeof(c_zero), &c_zero);
          cllog << "Target Buffer ..." << sizeof(work.targetBin);
          queue_.enqueueWriteBuffer(bufferTarget_, CL_FALSE, 0, sizeof(work.targetBin), work.targetBin.data());
          cllog << "Loaded";

          kernelSearch_.setArg(0, bufferSearch_);  // Supply output buffer to kernel.
          kernelSearch_.setArg(4, bufferTarget_);

          startNonce  = nonceStart_.load();
          cllog << "Nonce loaded" << startNonce;

          auto switchEnd = std::chrono::high_resolution_clock::now();
          auto globalSwitchTime = std::chrono::duration_cast<std::chrono::milliseconds>(switchEnd - workSwitchStart).count();
          auto localSwitchTime = std::chrono::duration_cast<std::chrono::microseconds>(switchEnd - localSwitchStart).count();
          cllog << "Switch time" << globalSwitchTime << "ms /" << localSwitchTime << "us";
        }

        // Run the kernel.
        kernelSearch_.setArg(3, startNonce);
        queue_.enqueueNDRangeKernel(kernelSearch_, cl::NullRange, globalWorkSize_, workgroupSize_);

        // Read results.
        // TODO: could use pinned host pointer instead.
        uint32_t results[c_maxSearchResults + 1];
        queue_.enqueueReadBuffer(bufferSearch_, CL_TRUE, 0, sizeof(results), &results);
        //cllog << "results[0]: " << results[0] << " [1]: " << results[1];

        uint64_t nonce = 0;
        if (results[0] > 0)
        {
          // Ignore results except the first one.
          nonce = startNonce + results[1];
          // Reset search buffer if any solution found.
          queue_.enqueueWriteBuffer(bufferSearch_, CL_FALSE, 0, sizeof(c_zero), &c_zero);
        }


        // Report results while the kernel is running.
        // It takes some time because ethash must be re-evaluated on CPU.
        if (nonce != 0)
        {
          // Report hash count
          cllog << "Global work" << globalWorkSize_;
          cllog << "Nonce: " << nonce ;
          auto nonceForHash = be32dec(&nonce);
          *(work.blockHeader.data() + 20) = nonceForHash;

          Solution solution(work);
          plant_.submit(solution);
        }

        addHashCount(globalWorkSize_);

        // Increase start nonce for following kernel execution.
        startNonce += globalWorkSize_;

        // Check if we should stop.
        if (isStopped())
        {
          // Make sure the last buffer write has finished --
          // it reads local variable.
          queue_.finish();
          break;
        }
      }
    }
    catch (cl::Error const& _e)
    {
      cwarn << "OpenCL Error:" << _e.what() << _e.err();
    }
  }


  std::tuple<bool, cl::Device, int, int, std::string> OpenCLMiner::getDeviceInfo(int index)
  {
    auto failResult = std::make_tuple(false, cl::Device(), 0, 0, "");
    std::vector<cl::Platform> platforms = getPlatforms();
    if (platforms.empty())
    {
      return failResult;
    }

    // use selected platform
    unsigned platformIdx = std::min<unsigned>(s_platformId, platforms.size() - 1);
    std::string platformName = platforms[platformIdx].getInfo<CL_PLATFORM_NAME>();
    ETHCL_LOG("Platform: " << platformName);

    int platformId = OPENCL_PLATFORM_UNKNOWN;
    if (platformName == "NVIDIA CUDA")
    {
      platformId = OPENCL_PLATFORM_NVIDIA;
    }
    else if (platformName == "AMD Accelerated Parallel Processing")
    {
      platformId = OPENCL_PLATFORM_AMD;
    }
    else if (platformName == "Clover")
    {
      platformId = OPENCL_PLATFORM_CLOVER;
    }

    // get GPU device of the default platform
    std::vector<cl::Device> devices = getDevices(platforms, platformIdx);
    if (devices.empty())
    {
      ETHCL_LOG("No OpenCL devices found.");
      return failResult;
    }

    // use selected device
    unsigned deviceId = s_devices[index] > -1 ? s_devices[index] : index;
    cl::Device& device = devices[std::min<unsigned>(deviceId, devices.size() - 1)];
    std::string device_version = device.getInfo<CL_DEVICE_VERSION>();
    ETHCL_LOG("Device:   " << device.getInfo<CL_DEVICE_NAME>() << " / " << device_version);

    std::string clVer = device_version.substr(7, 3);

    if (clVer == "1.0" || clVer == "1.1")
    {
      if (platformId == OPENCL_PLATFORM_CLOVER)
      {
        ETHCL_LOG("OpenCL " << clVer << " not supported, but platform Clover might work nevertheless. USE AT OWN RISK!");
      }
      else
      {
        ETHCL_LOG("OpenCL " << clVer << " not supported - minimum required version is 1.2");
        return failResult;
      }
    }

    char options[256];
    int computeCapability = 0;
    if (platformId == OPENCL_PLATFORM_NVIDIA)
    {
      cl_uint computeCapabilityMajor;
      cl_uint computeCapabilityMinor;
      clGetDeviceInfo(device(), CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV, sizeof(cl_uint), &computeCapabilityMajor, NULL);
      clGetDeviceInfo(device(), CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV, sizeof(cl_uint), &computeCapabilityMinor, NULL);

      computeCapability = computeCapabilityMajor * 10 + computeCapabilityMinor;
      int maxregs = computeCapability >= 35 ? 72 : 63;
      sprintf(options, "-cl-nv-maxrregcount=%d", maxregs);
    }
    else
    {
      sprintf(options, "%s", "");
    }

    return std::make_tuple(true, device, platformId, computeCapability, std::string(options));
  }


  bool OpenCLMiner::init_dag(const std::string &seed, uint32_t height)
  {
    uint8_t seedInput[32];
    std::memcpy(seedInput, reinterpret_cast<const uint8_t*>(seed.c_str()), sizeof(seedInput));


    // get all platforms
    try
    {
      auto deviceResult = getDeviceInfo(index_);

      // create context
      auto device = std::get<1>(deviceResult);
      context_  = cl::Context(device);
      queue_    = cl::CommandQueue(context_, device);

      // make sure that global work size is evenly divisible by the local workgroup size
      workgroupSize_        = s_workgroupSize;
      globalWorkSize_       = s_initialGlobalWorkSize;
      if (globalWorkSize_ % workgroupSize_ != 0)
      {
        globalWorkSize_ = ((globalWorkSize_ / workgroupSize_) + 1) * workgroupSize_;
      }

      // patch source code
      // note: CLMiner_kernel is simply ethash_cl_miner_kernel.cl compiled
      // into a byte array by bin2h.cmake. There is no need to load the file by hand in runtime
      // TODO: Just use C++ raw string literal.
      std::string code(CLMiner_kernel, CLMiner_kernel + sizeof(CLMiner_kernel));

      egihash::cache_t cache = egihash::cache_t(height, seed);
      uint64_t dagSize = egihash::dag_t::get_full_size(height);
      uint32_t dagSize128 = (unsigned)(dagSize / egihash::constants::MIX_BYTES);
      uint32_t lightSize64 = (unsigned)(cache.data().size());

      addDefinition(code, "GROUP_SIZE", workgroupSize_);
      addDefinition(code, "DAG_SIZE", dagSize128);
      addDefinition(code, "LIGHT_SIZE", lightSize64);
      addDefinition(code, "ACCESSES", egihash::constants::ACCESSES);
      addDefinition(code, "MAX_OUTPUTS", c_maxSearchResults);
      addDefinition(code, "PLATFORM", std::get<2>(deviceResult));
      addDefinition(code, "COMPUTE", std::get<3>(deviceResult));
      addDefinition(code, "THREADS_PER_HASH", s_threadsPerHash);

      // create miner OpenCL program
      cl::Program::Sources sources{{code.data(), code.size()}};
      cl::Program program(context_, sources);
      try
      {
        program.build({device}, std::get<4>(deviceResult).c_str());
        cllog << "Build info:" << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
      }
      catch (cl::Error const&)
      {
        cwarn << "Build info:" << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
        cwarn << " FAiled" ;
        return false;
      }

      // create buffer for dag
      std::vector<uint32_t> vData;
      for ( auto &d : cache.data() )
      {
        for ( auto &dv : d)
        {
          vData.push_back(dv.hword);
        }

      }

      try
      {
        bufferLight_      = cl::Buffer(context_, CL_MEM_READ_ONLY, sizeof(uint32_t) * vData.size());
        bufferDag_        = cl::Buffer(context_, CL_MEM_READ_ONLY, dagSize);

        kernelSearch_     = cl::Kernel(program, "ethash_search");
        kernelDag_        = cl::Kernel(program, "ethash_calculate_dag_item");

        ETHCL_LOG("Creating light buffer");
        queue_.enqueueWriteBuffer(bufferLight_, CL_TRUE, 0, sizeof(uint32_t) * vData.size(), vData.data());
      }
      catch (cl::Error const& err)
      {
        cwarn << "Creating DAG buffer failed:" << err.what() << err.err();
        return false;
      }

      // create buffer for header
      ETHCL_LOG("Creating buffer for header.");
      bufferHeader_ = cl::Buffer(context_, CL_MEM_READ_ONLY, 32);

      kernelSearch_.setArg(1, bufferHeader_);
      kernelSearch_.setArg(2, bufferDag_);
      kernelSearch_.setArg(5, ~0u);  // Pass this to stop the compiler unrolling the loops.

      // create mining buffers
      ETHCL_LOG("Creating mining buffer");
      bufferSearch_ = cl::Buffer(context_, CL_MEM_WRITE_ONLY, (c_maxSearchResults + 1) * sizeof(uint32_t));
      bufferTarget_ = cl::Buffer(context_, CL_MEM_READ_ONLY, 32);

      cllog << "Generating DAG";

      uint32_t const work = (uint32_t)(dagSize / sizeof(egihash::node));
      uint32_t fullRuns = work / globalWorkSize_;
      uint32_t const restWork = work % globalWorkSize_;
      if (restWork > 0) fullRuns++;

      kernelDag_.setArg(1, bufferLight_);
      kernelDag_.setArg(2, bufferDag_);
      kernelDag_.setArg(3, ~0u);

      for (uint32_t i = 0; i < fullRuns; i++)
      {
        kernelDag_.setArg(0, i * globalWorkSize_);
        queue_.enqueueNDRangeKernel(kernelDag_, cl::NullRange, globalWorkSize_, workgroupSize_);
        queue_.finish();
        //cllog << "DAG" << int(100.0f * i / fullRuns) << '%';
      }

      cllog << "DAG Loaded" ;


//      // Test DAG Read
//      uint64_t results[8];
//      queue_.enqueueReadBuffer(bufferDag_, CL_TRUE, 0, sizeof(results), &results);
//      for( int i = 0; i < 8; ++ i)
//      {
//        std::cerr<< "DAG  " << i << " " << std::hex << results[i];
//      }

      std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    catch (cl::Error const& err)
    {
      cwarn << err.what() << "(" << err.err() << ")";
      return false;
    }
    return true;
  }

} /* namespace energi */
