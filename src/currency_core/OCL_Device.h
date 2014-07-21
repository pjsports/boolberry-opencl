#include <CL/cl.h>
#include <map>

#define OCL_DEVICE_OUTPUT_SIZE 16

class OCL_Device
{
private:
	unsigned int m_platform_num;
	unsigned int m_device_num;
	cl_platform_id		m_platform_id;
	cl_device_id		m_device_id;
	cl_context			m_context;
	cl_command_queue	m_queue;

	cl_kernel kernel;
	cl_kernel initKernel;
	cl_kernel init2Kernel;
	cl_kernel mixinKernel;
	cl_kernel rndKernel;
	cl_kernel resultKernel;
	cl_mem scratchpadBuffer;
	cl_mem inputBuffer;
	cl_mem outputBuffer;
	cl_mem stateBuffer;

	char* m_sBuildOptions;

	unsigned int work_size;
	unsigned int scratchpad_size;
	unsigned int m_type;
	bool ok;

	// Uniquely maps a kernel via a program name and a kernel name
	std::map<std::string, std::pair<cl_program, 
		std::map<std::string, cl_kernel> > > m_kernels;
	
	// Maps each buffer to a index
	std::map<int, cl_mem> m_buffers;

	cl_program GetProgram(const char* sProgramName);
	void OCL_Device::PrintPlatformInfo(cl_platform_id platformId);
	void OCL_Device::PrintDeviceInfo(cl_device_id deviceId);
	std::string OCL_Device::GetFileContents(const char *filename);

public:
	OCL_Device(unsigned int iPlatformNum, unsigned int iDeviceNum, unsigned int workSize, unsigned int iKernelType, const char* sKernel);
	~OCL_Device(void);

	void SetBuildOptions (const char* sBuildOptions);
	cl_kernel GetKernel (const char* sProgramName, const char* sKernelName);
	cl_mem DeviceMalloc(int idx, size_t size);
	void CopyBufferToDevice(void* h_Buffer, int idx, size_t size);
	void CopyBufferToHost  (void* h_Buffer, int idx, size_t size);

	cl_command_queue GetQueue();
	bool isOK();

	void PrintInfo(void);
	void OCL_Device::CHECK_OPENCL_ERROR(cl_int err);
	
	void OCL_Device::run(size_t offset, cl_ulong target);
	void OCL_Device::updateScratchpad(void* m_scratchpad, size_t size, int hashSize);

};

