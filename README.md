Boolberry OpenCL miner configuration
------------------------------------
### OpenCL solo miner/daemon (boolbd) 
It supports ordinary mining parameters:  
--start-mining=[address]  
--mining-threads=[n], n - number of GPUs to use

Run the daemon once to create miner_conf.json in the data directory.

Keys to tune the solo miner: 
  "platform_index" - use OpenCL platform (if you have manym, default 0)  
  "device_index" - use OpenCL devices from (if you have many, default from 0)  
  "difficulty" - virtual share difficulty for statistics  
  "donation_percent_opencl" - OpenCL developer donation, default value - 2%  
  "kernel" - OpenCL kernel file name  
  "kernel_type" - kernel type: 0 - all-in-one, 1 - multiple step version (wild_keccak-multi.cl)  
  "thread_delay" - delay in ms between thread startups  
  "work_size" - OpenCL work size (smaller values make the computer more responsive)  

### OpenCL pool miner (simpleminer)
It works with JSON RPC (http) pools only and supports ordinary mining parameters:  
--pool-addr [pool url]  
--login [usually your wallet address] 
--pass [usually any]  
--mining-threads 1 (don't change it!)  
GPU specific parameters are:  
--platform [n] OpenCL platform index (default 0)  
--device [n] OpenCL device index (default 0)

Start one instance per GPU and increment device index for every new instance. You will not get performance close to solo miner due to network delays.

**Performance (solo):**  
6870, R270x - 500-600 khs  
6970, R280X - 900-1000 khs  
GTX 560 Ti - 335 khs  
AWS g2.2 (GTX 660 Ti) - 365 khs

OpenCL code is available under the terms of the GNU Public License version 3.  
Author Mikhail Kuperman (mbk.git@gmail.com)  
BBR donation address: 1EmWGnwhydr3S2vRWQbbefh1hgDKgMjdMGe43ZgdPhdARhNBRkUMuD4YzLA2nyYG8tg2HKCCBg4aDamJKypRQWW1Ca2kSV8  
BTC donation address: 1Lns6UjL3sw77DJ5z1EKJZy6SnqriqvVGK

Building
--------

### Unix and MacOS X

Dependencies: GCC 4.7.3 or later, CMake 2.8.6 or later, and Boost 1.53(but don't use 1.54) or later. You may download them from:
http://gcc.gnu.org/  
http://www.cmake.org/  
http://www.boost.org/  
Alternatively, it may be possible to install them using a package manager.

OpenCL SDK (you can install CUDA too):  
http://developer.amd.com/tools-and-sdks/opencl-zone/opencl-tools-sdks/amd-accelerated-parallel-processing-app-sdk/

Set AMDAPPSDKROOT or CUDA_PATH if they have not been set already depending on SDK you installed.

More detailed instructions for OS X (assume you’re using MacPorts (they’re, however, pretty self-explanatory and homebrew users shouldn't have troubles following it too):

* Install latest Xcode and command line tools (these are in fact MacPorts prerequisites)
* Install GCC 4.8 and toolchain selector tool: `sudo port install gcc48 gcc_select`
* Set GCC 4.8 as an active compiler toolchain: `sudo port select --set gcc mp-gcc48`
* Install latest Boost, CMake: `sudo port install boost cmake`

To build, change to a directory where this file is located, and run `make`. The resulting executables can be found in `build/release/src`.  
Copy `*.cl` kernels from `src/cl` to executable's directory.

**Advanced options**:

Parallel build: run `make -j<number of threads>` instead of just `make`.  
Debug build: run `make build-debug`.
Test suite: run `make test-release` to run tests in addition to building. Running `make test-debug` will do the same to the debug version.

Building with Clang: it may be possible to use Clang instead of GCC, but this may not work everywhere. To build, run `export CC=clang CXX=clang++` before running `make`.

### Windows

Dependencies: MSVC 2012, CMake 2.8.6 or later, and Boost 1.55. 

You may download them from:  
http://www.microsoft.com/en-US/download/details.aspx?id=34673  
http://www.cmake.org/files/v3.0/cmake-3.0.0-win32-x86.exe  
http://sourceforge.net/projects/boost/files/boost-binaries/1.55.0-build2/boost_1_55_0-msvc-11.0-64.exe/download

OpenCL SDK (you can install CUDA too):  
http://developer.amd.com/tools-and-sdks/opencl-zone/opencl-tools-sdks/amd-accelerated-parallel-processing-app-sdk/

To build, edit paths to Boost in `cmake-vs2012.bat` and run it.  
Open the generated `build/boolberry.sln` in VS2012, change the solution configuration to `Release` and build the targets you need in the prog group.  
You'll find executables in `build/src/Release` directory.  
Copy `*.cl` kernels from `src/cl` to executable's directory.

Good luck!
