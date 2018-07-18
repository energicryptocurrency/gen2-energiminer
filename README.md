# energiminer
Universal CPU/GPU Miner for egihash 

### Features

- OpenCL mining
- Nvidia CUDA mining
- realistic benchmarking against arbitrary epoch/DAG/blocknumber
- on-GPU DAG generation (no more DAG files on disk)
- stratum mining without proxy
- OpenCL devices picking
- farm failover (getwork + stratum)


## Table of Contents

- [Usage](#usage)
- [Build](#build)
  - [Continuous Integration and development builds](#continuous-integration-and-development-builds)
  - [Building from source](#building-from-source)
  - [CMake configuration options](#cmake-configuration-options)
- [F.A.Q.](#faq)


## Usage

### Continuous Integration and development builds

| CI            | OS            | Status  | Development builds |
| ------------- | ------------- | -----   | -----------------  |
| [AppVeyor]    | Windows       | [![AppVeyor](https://img.shields.io/appveyor/ci/levon/energiminer.svg)][AppVeyor] | ✓ Build artifacts available for all PRs and branches |

The AppVeyor system automatically builds a Windows .exe for every commit. The latest version is always available [on the landing page](https://ci.appveyor.com/project/levon/energiminer) or you can [browse the history](https://ci.appveyor.com/project/levon/energiminer/history) to access previous builds.

To download the .exe on a build under `JOB NAME` select `Configuration: Release`, choose `ARTIFACTS` then download the zip file.

The **energiminer** is a command line program. This means you launch it either
from a Windows command prompt or Linux console, or create shortcuts to
predefined command lines using a Linux Bash script or Windows batch/cmd file.
For a full list of available command, please run

```sh
energiminer --help
```

## Build
### Building from source

This project uses [CMake] and [Hunter] package manager.

To avoid know issue "parse error in template argument list" please update your gcc/g++ compiler to 6th version.

1. Create a build directory.

   ```sh
   mkdir build; cd build
   ```

2. Configure the project with CMake. Check out additional
   [configuration options](#cmake-configuration-options).

   ```sh
   cmake ..
   ```

   Note: In Windows, it is possible to have issues with VS 2017 compilers, in that case, use VS 2017 installer to get VS 2015 compilers and use:

   ```sh
   cmake .. -G "Visual Studio 15 2017 Win64" -Tv140
   ```

3. Build the project using [CMake Build Tool Mode]. This is a portable variant
   of `make`.

   ```sh
   cmake --build .
   ```

   Note: In Windows, it is possible to have compiler issues if you don't specify build config. In that case use:

   ```sh
   cmake --build . --config Release
   ```

5. _(Optional, Linux only)_ Install the built executable.

   ```sh
   sudo make install
   ```

#### OpenCL support on Linux

If you're planning to use [OpenCL on Linux](https://github.com/ruslo/hunter/wiki/pkg.opencl#pitfalls)
you have to install OpenGL libraries. E.g. on Ubuntu run:

```sh
sudo apt-get install mesa-common-dev
```

#### Disable Hunter

If you want to install dependencies yourself or use system package manager
you can disable Hunter by adding
[-DHUNTER_ENABLED=OFF](https://docs.hunter.sh/en/latest/reference/user-variables.html#hunter-enabled)
to configuration options.

### CMake configuration options

Pass these options to CMake configuration command, e.g.

```sh
cmake .. -DETHASHCUDA=ON -DETHASHCL=OFF
```

- `-DETHASHCL=ON` - enable OpenCL mining, `ON` by default,
- `-DETHASHCUDA=ON` - enable CUDA mining, `OFF` by default.

## License

Licensed under the [GNU General Public License, Version 3](LICENSE).


## F.A.Q

### Why is my hashrate with Nvidia cards on Windows 10 so low?

The new WDDM 2.x driver on Windows 10 uses a different way of addressing the GPU. This is good for a lot of things, but not for ETH mining.

* For Kepler GPUs: I actually don't know. Please let me know what works best for good old Kepler.
* For Maxwell 1 GPUs: Unfortunately the issue is a bit more serious on the GTX750Ti, already causing suboptimal performance on Win7 and Linux. Apparently about 4MH/s can still be reached on Linux, which, depending on ETH price, could still be profitable, considering the relatively low power draw.
* For Maxwell 2 GPUs: There is a way of mining ETH at Win7/8/Linux speeds on Win10, by downgrading the GPU driver to a Win7 one (350.12 recommended) and using a build that was created using CUDA 6.5.
* For Pascal GPUs: You have to use the latest WDDM 2.1 compatible drivers in combination with Windows 10 Anniversary edition in order to get the full potential of your Pascal GPU.

### Why is a GTX 1080 slower than a GTX 1070?

Because of the GDDR5X memory, which can't be fully utilized for ETH mining (yet).

### Are AMD cards also affected by slowdowns with increasing DAG size?

Only GCN 1.0 GPUs (78x0, 79x0, 270, 280), but in a different way. You'll see that on each new epoch (30K blocks), the hashrate will go down a little bit.

### Can I still mine ETH with my 2GB GPU?

Not really, your VRAM must be above the DAG size (Currently about 2.15 GB.) to get best performance. Without it severe hash loss will occur.

### What are the optimal launch parameters?

The default parameters are fine in most scenario's (CUDA). For OpenCL it varies a bit more. Just play around with the numbers and use powers of 2. GPU's like powers of 2.

### What does the `--cuda-parallel-hash` flag do?

[@davilizh](https://github.com/davilizh) made improvements to the CUDA kernel hashing process and added this flag to allow changing the number of tasks it runs in parallel. These improvements were optimised for GTX 1060 GPUs which saw a large increase in hashrate, GTX 1070 and GTX 1080/Ti GPUs saw some, but less, improvement. The default value is 4 (which does not need to be set with the flag) and in most cases this will provide the best performance.

### What is energiminer's relationship with [Genoil's fork]?

[Genoil's fork] was the original source of this version, but as Genoil is no longer consistently maintaining that fork it became almost impossible for developers to get new code merged there. In the interests of progressing development without waiting for reviews this fork should be considered the active one and Genoil's as legacy code.

### CUDA GPU order changes sometimes. What can I do?

There is an environment var `CUDA_DEVICE_ORDER` which tells the Nvidia CUDA driver how to enumerates the graphic cards.
The following values are valid:

* `FASTEST_FIRST` (Default) - causes CUDA to guess which device is fastest using a simple heuristic.
* `PCI_BUS_ID` - orders devices by PCI bus ID in ascending order.

To prevent some unwanted changes in the order of your CUDA devices you **might set the environment variable to `PCI_BUS_ID`**.
This can be done with one of the 2 ways:

* Linux:
    * Adapt the `/etc/environment` file and add a line `CUDA_DEVICE_ORDER=PCI_BUS_ID`
    * Adapt your start script launching energiminer and add a line `export CUDA_DEVICE_ORDER=PCI_BUS_ID`

* Windows:
    * Adapt your environment using the control panel (just search `setting environment windows control panel` using your favorite search engine)
    * Adapt your start (.bat) file launching energiminer and add a line `set CUDA_DEVICE_ORDER=PCI_BUS_ID` or `setx CUDA_DEVICE_ORDER PCI_BUS_ID`. For more info about `set` see [here](https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/set_1), for more info about `setx` see [here](https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/setx)

10. Insufficient CUDA driver

    ```shell
    Error: Insufficient CUDA driver: 9010
    ```

    You have to upgrade your Nvidia drivers. On Linux, install `nvidia-396` package or newer.

[AppVeyor]: https://img.shields.io/appveyor/ci/levon/energiminer.svg
[Releases]: https://github.com/levongh/energiminer/releases
