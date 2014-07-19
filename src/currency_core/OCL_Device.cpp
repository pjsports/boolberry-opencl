#include <CL\opencl.h>
#include "OCL_Device.h"

#define ENABLE_RELEASE_LOGGING
#include "misc_log_ex.h"

OCL_Device::OCL_Device(unsigned int iPlatformNum, unsigned int iDeviceNum, unsigned int iWorkSize, unsigned int iKernelType, const char* sKernel)
{
	ok = false;
	m_platform_num = iPlatformNum;
	m_device_num = iDeviceNum;
	m_type = iKernelType;
	LOG_PRINT_L0("[OCL " << m_platform_num << "/" << m_device_num << "] start with kernel " <<sKernel << ", type " << m_type);

	// For error checking
	cl_int err;

	// Get Platform Info
	LOG_PRINT_L2("[OCL " << m_platform_num << "/" << m_device_num << "] get Platform Info");
	cl_uint iNumPlatforms = 0;
	err = clGetPlatformIDs(NULL, NULL, &iNumPlatforms); 
	CHECK_OPENCL_ERROR(err);

	cl_platform_id* vPlatformIDs = 
		(cl_platform_id *) new cl_platform_id[iNumPlatforms];
	err = clGetPlatformIDs(iNumPlatforms, vPlatformIDs, NULL); 
	CHECK_OPENCL_ERROR(err);
	if (iPlatformNum >= iNumPlatforms)
	{
		LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] platform index must me between 0 and " << (iNumPlatforms-1));
		delete[] vPlatformIDs;
		return;
	}
	m_platform_id = vPlatformIDs[iPlatformNum];
	delete[] vPlatformIDs;

	// Get Device Info
	LOG_PRINT_L2("[OCL " << m_platform_num << "/" << m_device_num << "] get Device Info");
	cl_uint iNumDevices = 0;
	err = clGetDeviceIDs(m_platform_id, CL_DEVICE_TYPE_GPU, NULL, NULL, 
		&iNumDevices); 
	CHECK_OPENCL_ERROR(err);

	cl_device_id* vDeviceIDs = (cl_device_id*) new cl_device_id[iNumDevices];	
	err = clGetDeviceIDs(m_platform_id, CL_DEVICE_TYPE_GPU, iNumDevices, 
		vDeviceIDs, &iNumDevices); 
	CHECK_OPENCL_ERROR(err);
	if (iDeviceNum >= iNumDevices)
	{
		LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] device index must me between 0 and " << (iNumDevices-1));
		delete[] vDeviceIDs;
		return;
	}
	m_device_id = vDeviceIDs[iDeviceNum];
	delete[] vDeviceIDs;

	LOG_PRINT_L2("[OCL " << m_platform_num << "/" << m_device_num << "] create context");
	cl_context_properties vProprieties[3] = {CL_CONTEXT_PLATFORM, 
		(cl_context_properties)m_platform_id, 0};
	m_context = clCreateContext(vProprieties, 1, &m_device_id, NULL, NULL, 
		&err); 
	CHECK_OPENCL_ERROR(err);

	LOG_PRINT_L2("[OCL " << m_platform_num << "/" << m_device_num << "] create command queue");
	m_queue = clCreateCommandQueue(m_context, m_device_id, NULL, &err); 
	CHECK_OPENCL_ERROR(err);

	char* m_sBuildOptions = "";

	LOG_PRINT_L2("[OCL " << m_platform_num << "/" << m_device_num << "] init kernels");
	if (m_type == 0)
		kernel = GetKernel(sKernel, "search");
	else if (m_type == 1) {
		initKernel = GetKernel(sKernel, "init");
		init2Kernel = GetKernel(sKernel, "init2");
		rndKernel = GetKernel(sKernel, "rnd");
		mixinKernel = GetKernel(sKernel, "mixin");
		resultKernel = GetKernel(sKernel, "result");
	}
	else {
		LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] kernel type " << m_type << " not supported");
		return;
	}
	LOG_PRINT_L2("[OCL " << m_platform_num << "/" << m_device_num << "] allocate memory");
	inputBuffer = DeviceMalloc(0, 256);
	outputBuffer = DeviceMalloc(1, OCL_DEVICE_OUTPUT_SIZE * sizeof(cl_ulong));
	scratchpadBuffer = DeviceMalloc(2, 32*1024*1024);
	if (m_type == 1)
		stateBuffer = DeviceMalloc(3, iWorkSize * 8 * 25);
	work_size = iWorkSize;
	ok = true;
	LOG_PRINT_L2("[OCL " << m_platform_num << "/" << m_device_num << "] success");
}

void OCL_Device::updateScratchpad(void* scratchpad, size_t size, int hashSize)
{
	LOG_PRINT_L2("[OCL " << m_platform_num << "/" << m_device_num << "] copy scratchpad");
	CopyBufferToDevice(scratchpad, 2, size * hashSize);	
	scratchpad_size = (unsigned int)size;
}

void OCL_Device::run(size_t offset, cl_ulong target)
{
	LOG_PRINT_L3("[OCL " << m_platform_num << "/" << m_device_num << "] run");
	size_t off = offset;
	size_t num = work_size;
	size_t threads = 256;

	cl_int err;
	if (m_type == 0) {
		err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &inputBuffer);   
		CHECK_OPENCL_ERROR(err);
		err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &outputBuffer);   
		CHECK_OPENCL_ERROR(err);
		err = clSetKernelArg(kernel, 2, sizeof(cl_mem), &scratchpadBuffer);   
		CHECK_OPENCL_ERROR(err);
		err = clSetKernelArg(kernel, 3, sizeof(scratchpad_size), &scratchpad_size);   
		CHECK_OPENCL_ERROR(err);
		cl_ulong targetArg = target;
		err = clSetKernelArg(kernel, 4, sizeof(targetArg), &targetArg);   
		CHECK_OPENCL_ERROR(err);
		err = clEnqueueNDRangeKernel(GetQueue(), kernel, 1, &off, &num, &threads, 0, NULL, NULL);
		CHECK_OPENCL_ERROR(err);
	} else if (m_type == 1) {
		size_t num2 = work_size * 6;
		err = clSetKernelArg(initKernel, 0, sizeof(cl_mem), &inputBuffer);   
		CHECK_OPENCL_ERROR(err);
		err = clSetKernelArg(initKernel, 1, sizeof(cl_mem), &stateBuffer);   
		CHECK_OPENCL_ERROR(err);
		err = clEnqueueNDRangeKernel(GetQueue(), initKernel, 1, &off, &num, &threads, 0, NULL, NULL);
		CHECK_OPENCL_ERROR(err);

		for (cl_int round = 0; round < 24; round++) {
			if (round != 0) {
				err = clSetKernelArg(mixinKernel, 0, sizeof(cl_mem), &stateBuffer);   
				CHECK_OPENCL_ERROR(err);
				err = clSetKernelArg(mixinKernel, 1, sizeof(cl_mem), &scratchpadBuffer);   
				CHECK_OPENCL_ERROR(err);
				err = clSetKernelArg(mixinKernel, 2, sizeof(scratchpad_size), &scratchpad_size);   
				CHECK_OPENCL_ERROR(err);
				err = clEnqueueNDRangeKernel(GetQueue(), mixinKernel, 1, NULL, &num2, &threads, 0, NULL, NULL);
				CHECK_OPENCL_ERROR(err);
			}
			err = clSetKernelArg(rndKernel, 0, sizeof(cl_mem), &stateBuffer);   
			CHECK_OPENCL_ERROR(err);
			err = clSetKernelArg(rndKernel, 1, sizeof(round), &round);   
			CHECK_OPENCL_ERROR(err);
			err = clEnqueueNDRangeKernel(GetQueue(), rndKernel, 1, &off, &num, &threads, 0, NULL, NULL);
			CHECK_OPENCL_ERROR(err);
		}

		err = clSetKernelArg(init2Kernel, 0, sizeof(cl_mem), &stateBuffer);   
		CHECK_OPENCL_ERROR(err);
		err = clEnqueueNDRangeKernel(GetQueue(), init2Kernel, 1, &off, &num, &threads, 0, NULL, NULL);
		CHECK_OPENCL_ERROR(err);
		for (cl_int round = 0; round < 24; round++) {
			if (round != 0) {
				err = clSetKernelArg(mixinKernel, 0, sizeof(cl_mem), &stateBuffer);   
				CHECK_OPENCL_ERROR(err);
				err = clSetKernelArg(mixinKernel, 1, sizeof(cl_mem), &scratchpadBuffer);   
				CHECK_OPENCL_ERROR(err);
				err = clSetKernelArg(mixinKernel, 2, sizeof(scratchpad_size), &scratchpad_size);   
				CHECK_OPENCL_ERROR(err);
				err = clEnqueueNDRangeKernel(GetQueue(), mixinKernel, 1, NULL, &num2, &threads, 0, NULL, NULL);
				CHECK_OPENCL_ERROR(err);
			}
			err = clSetKernelArg(rndKernel, 0, sizeof(cl_mem), &stateBuffer);   
			CHECK_OPENCL_ERROR(err);
			err = clSetKernelArg(rndKernel, 1, sizeof(round), &round);   
			CHECK_OPENCL_ERROR(err);
			err = clEnqueueNDRangeKernel(GetQueue(), rndKernel, 1, &off, &num, &threads, 0, NULL, NULL);
			CHECK_OPENCL_ERROR(err);
		}

		err = clSetKernelArg(resultKernel, 0, sizeof(cl_mem), &outputBuffer);   
		CHECK_OPENCL_ERROR(err);
		err = clSetKernelArg(resultKernel, 1, sizeof(cl_mem), &stateBuffer);   
		CHECK_OPENCL_ERROR(err);
		cl_ulong targetArg = target;
		err = clSetKernelArg(resultKernel, 2, sizeof(targetArg), &targetArg);   
		CHECK_OPENCL_ERROR(err);
		err = clEnqueueNDRangeKernel(GetQueue(), resultKernel, 1, &off, &num, &threads, 0, NULL, NULL);
		CHECK_OPENCL_ERROR(err);
	}

	err = clFinish(GetQueue());
	CHECK_OPENCL_ERROR(err);
}

bool OCL_Device::isOK()
{
	return ok;
}

void OCL_Device::PrintInfo()
{	
	// Printing device and platform information
	PrintPlatformInfo(m_platform_id);
	PrintDeviceInfo(m_device_id);
}

cl_program OCL_Device::GetProgram (const char* sProgramName)
{
	std::string sFile = GetFileContents(sProgramName);
	const char* pFile = sFile.c_str();
	const size_t lFile = sFile.length();

	cl_int err;
	cl_program program = clCreateProgramWithSource(m_context, 1, 
		(const char** const)&pFile, &lFile, &err); 
	CHECK_OPENCL_ERROR(err);

	err = clBuildProgram(program, 1, &m_device_id, NULL, NULL, NULL);
	if (err != CL_SUCCESS)
	{
		size_t size;
		clGetProgramBuildInfo (program, m_device_id, 
			CL_PROGRAM_BUILD_LOG, 0, NULL, &size);
		char* sLog = (char*) malloc (size);
		clGetProgramBuildInfo (program, m_device_id, 
			CL_PROGRAM_BUILD_LOG, size, sLog, NULL);

		LOG_ERROR("[OCL " << m_platform_num << "/" << m_device_num << "] build log: " << sLog);
		free(sLog);

		CHECK_OPENCL_ERROR(err);
	}

	return program;
}

void OCL_Device::SetBuildOptions(const char* sBuildOptions)
{
	size_t iLen = strlen(sBuildOptions);
	m_sBuildOptions = (char*) malloc (iLen + 1);
	strncpy(m_sBuildOptions, sBuildOptions, iLen);
}

cl_kernel OCL_Device::GetKernel (const char* sProgramName, 
								 const char* sKernelName)
{
	if (m_kernels.find(sProgramName) == m_kernels.end())
	{
		// Build program
		cl_program program = GetProgram(sProgramName);

		// Add to map
		m_kernels[sProgramName] = std::pair<cl_program, std::map<std::string, 
			cl_kernel> >(program, std::map<std::string, cl_kernel>());
	}

	if (m_kernels[sProgramName].second.find(sKernelName) == 
		m_kernels[sProgramName].second.end())
	{
		// Create kernel
		cl_int err;
		cl_kernel kernel = clCreateKernel(m_kernels[sProgramName].first, 
			sKernelName, &err);     
		CHECK_OPENCL_ERROR(err);

		// Add to map
		m_kernels[sProgramName].second[sKernelName] = kernel;
	}

	return m_kernels[sProgramName].second[sKernelName];
}

cl_mem OCL_Device::DeviceMalloc(int idx, size_t size)
{
	cl_int err;
	if (m_buffers.find(idx) != m_buffers.end())
	{
		err = clReleaseMemObject(m_buffers[idx]);
		CHECK_OPENCL_ERROR(err);
	}

	cl_mem mem = clCreateBuffer(m_context, CL_MEM_READ_WRITE, size, NULL, 
		&err);
	CHECK_OPENCL_ERROR(err);

	m_buffers[idx] = mem;

	return mem;
}

cl_command_queue OCL_Device::GetQueue()
{
	return m_queue;
}


void OCL_Device::CopyBufferToDevice(void* h_Buffer, int idx, size_t size)
{

	cl_int err = clEnqueueWriteBuffer (m_queue, m_buffers[idx], CL_TRUE, 0, 
		size, h_Buffer, 0, NULL, NULL);
	CHECK_OPENCL_ERROR(err);
}

void OCL_Device::CopyBufferToHost  (void* h_Buffer, int idx, size_t size)
{
	cl_int err = clEnqueueReadBuffer (m_queue, m_buffers[idx], CL_TRUE, 0, 
		size, h_Buffer, 0, NULL, NULL);
	CHECK_OPENCL_ERROR(err);
}

void OCL_Device::PrintPlatformInfo(cl_platform_id platformId)
{
	cl_int err = 0;
	// Get Required Size
	size_t length;
	err = clGetPlatformInfo(platformId, CL_PLATFORM_NAME, NULL, NULL, &length);
	CHECK_OPENCL_ERROR(err);
	char* sInfo = new char[length];
	err = clGetPlatformInfo(platformId, CL_PLATFORM_NAME, length, sInfo, NULL);
	CHECK_OPENCL_ERROR(err);
	LOG_PRINT_L0("[OCL " << m_platform_num << "/" << m_device_num << "] using platform: " << sInfo);
	delete[] sInfo;
}

void OCL_Device::PrintDeviceInfo(cl_device_id deviceId)
{
	cl_int err = 0;

	// Get Required Size
	size_t length;
	err = clGetDeviceInfo(deviceId, CL_DEVICE_NAME, NULL, NULL, &length);
	// Get actual device name
	char* sInfo = new char[length];
	err = clGetDeviceInfo(deviceId, CL_DEVICE_NAME, length, sInfo, NULL);
	LOG_PRINT_L0("[OCL " << m_platform_num << "/" << m_device_num << "] using device:" << sInfo);
	delete[] sInfo;
}

std::string OCL_Device::GetFileContents(const char *filename)
{
	std::ifstream in(filename, std::ios::in | std::ios::binary);
	if (in)
	{
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		return(contents);
	}
	return "";
}

void OCL_Device::CHECK_OPENCL_ERROR(cl_int err)
{
	if (err != CL_SUCCESS)
	{
		switch (err)
		{
		case CL_DEVICE_NOT_FOUND: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_DEVICE_NOT_FOUND"); break;
		case CL_DEVICE_NOT_AVAILABLE:	
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_DEVICE_NOT_AVAILABLE"); break;
		case CL_COMPILER_NOT_AVAILABLE:	
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_COMPILER_NOT_AVAILABLE"); break;
		case CL_MEM_OBJECT_ALLOCATION_FAILURE:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_MEM_OBJECT_ALLOCATION_FAILURE"); break;
		case CL_OUT_OF_RESOURCES:				
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_OUT_OF_RESOURCES"); break;
		case CL_OUT_OF_HOST_MEMORY:				
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_OUT_OF_HOST_MEMORY"); break;
		case CL_PROFILING_INFO_NOT_AVAILABLE:	
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_PROFILING_INFO_NOT_AVAILABLE"); break;
		case CL_MEM_COPY_OVERLAP:				
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_MEM_COPY_OVERLAP"); break;
		case CL_IMAGE_FORMAT_MISMATCH:			
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_IMAGE_FORMAT_MISMATCH"); break;
		case CL_IMAGE_FORMAT_NOT_SUPPORTED:		
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_IMAGE_FORMAT_NOT_SUPPORTED"); break;
		case CL_BUILD_PROGRAM_FAILURE:			
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_BUILD_PROGRAM_FAILURE"); break;
		case CL_MAP_FAILURE:					
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_MAP_FAILURE"); break;
		case CL_MISALIGNED_SUB_BUFFER_OFFSET:	
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_MISALIGNED_SUB_BUFFER_OFFSET"); break;
		case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST"); break;
		case CL_COMPILE_PROGRAM_FAILURE: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_COMPILE_PROGRAM_FAILURE"); break;
		case CL_LINKER_NOT_AVAILABLE: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_LINKER_NOT_AVAILABLE"); break;
		case CL_LINK_PROGRAM_FAILURE: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_LINK_PROGRAM_FAILURE"); break;
		case CL_DEVICE_PARTITION_FAILED: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_DEVICE_PARTITION_FAILED"); break;
		case CL_KERNEL_ARG_INFO_NOT_AVAILABLE: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_KERNEL_ARG_INFO_NOT_AVAILABLE"); break;

		case CL_INVALID_VALUE: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_VALUE"); break;
		case CL_INVALID_DEVICE_TYPE: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_DEVICE_TYPE"); break;
		case CL_INVALID_PLATFORM:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_PLATFORM"); break;
		case CL_INVALID_DEVICE:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_DEVICE"); break;
		case CL_INVALID_CONTEXT:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_CONTEXT"); break;
		case CL_INVALID_QUEUE_PROPERTIES:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_QUEUE_PROPERTIES"); break;
		case CL_INVALID_COMMAND_QUEUE: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_COMMAND_QUEUE"); break;
		case CL_INVALID_HOST_PTR: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_HOST_PTR"); break;
		case CL_INVALID_MEM_OBJECT: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_MEM_OBJECT"); break;
		case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_IMAGE_FORMAT_DESCRIPTOR"); break;
		case CL_INVALID_IMAGE_SIZE:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_IMAGE_SIZE"); break;
		case CL_INVALID_SAMPLER:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_SAMPLER"); break;
		case CL_INVALID_BINARY:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_BINARY"); break;
		case CL_INVALID_BUILD_OPTIONS:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_BUILD_OPTIONS"); break;
		case CL_INVALID_PROGRAM: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_PROGRAM"); break;
		case CL_INVALID_PROGRAM_EXECUTABLE: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_PROGRAM_EXECUTABLE"); break;
		case CL_INVALID_KERNEL_NAME: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_KERNEL_NAME"); break;
		case CL_INVALID_KERNEL_DEFINITION: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_KERNEL_DEFINITION"); break;
		case CL_INVALID_KERNEL: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_KERNEL"); break;
		case CL_INVALID_ARG_INDEX:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_ARG_INDEX"); break;
		case CL_INVALID_ARG_VALUE:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_ARG_VALUE"); break;
		case CL_INVALID_ARG_SIZE: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_ARG_SIZE"); break;
		case CL_INVALID_KERNEL_ARGS: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_KERNEL_ARGS"); break;
		case CL_INVALID_WORK_DIMENSION: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_WORK_DIMENSION"); break;
		case CL_INVALID_WORK_GROUP_SIZE: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_WORK_GROUP_SIZE"); break;
		case CL_INVALID_WORK_ITEM_SIZE: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_WORK_ITEM_SIZE"); break;
		case CL_INVALID_GLOBAL_OFFSET: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_GLOBAL_OFFSET"); break;
		case CL_INVALID_EVENT_WAIT_LIST: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_EVENT_WAIT_LIST"); break;
		case CL_INVALID_EVENT:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_EVENT"); break;
		case CL_INVALID_OPERATION:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_OPERATION"); break;
		case CL_INVALID_GL_OBJECT: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_GL_OBJECT"); break;
		case CL_INVALID_BUFFER_SIZE: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_BUFFER_SIZE"); break;
		case CL_INVALID_MIP_LEVEL: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_MIP_LEVEL"); break;
		case CL_INVALID_GLOBAL_WORK_SIZE: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_GLOBAL_WORK_SIZE"); break;
		case CL_INVALID_PROPERTY:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_PROPERTY"); break;
		case CL_INVALID_IMAGE_DESCRIPTOR:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_IMAGE_DESCRIPTOR"); break;
		case CL_INVALID_COMPILER_OPTIONS:
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_COMPILER_OPTIONS"); break;
		case CL_INVALID_LINKER_OPTIONS: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_LINKER_OPTIONS"); break;
		case CL_INVALID_DEVICE_PARTITION_COUNT: 
			LOG_PRINT_RED_L0("[OCL " << m_platform_num << "/" << m_device_num << "] error:  CL_INVALID_DEVICE_PARTITION_COUNT"); break;
		}
		ok = false;
	}
}

OCL_Device::~OCL_Device(void)
{
	// Clean buffer for build options
	free(m_sBuildOptions);

	// Clean OpenCL Buffers
	for (std::map<int, cl_mem>::iterator it = m_buffers.begin() ; 
		it != m_buffers.end(); it++ )
	{
		// Release Buffer
		clReleaseMemObject(it->second);
	}

	// Clean OpenCL Programs and Kernels
	for (std::map<std::string, std::pair<cl_program, 
		std::map<std::string, cl_kernel> > >::iterator it = m_kernels.begin(); 
		it != m_kernels.end(); it++ )
	{
		for (std::map<std::string, cl_kernel>::iterator 
			it2 = it->second.second.begin(); 
			it2 != it->second.second.end(); it2++ )
		{
			clReleaseKernel(it2->second);
		}	
		clReleaseProgram(it->second.first);
	}	
}
