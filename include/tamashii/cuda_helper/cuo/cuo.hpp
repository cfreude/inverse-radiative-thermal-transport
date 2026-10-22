#pragma once
#include <cuda.h>
#include <cuda_runtime.h>
#include <optix.h>

/*! helper function that initializes optix and checks for errors */
//void SampleRenderer::initOptix()
//{
//    std::cout << "#osc: initializing optix..." << std::endl;
//
//    // -------------------------------------------------------
//    // check for available optix7 capable devices
//    // -------------------------------------------------------
//    cudaFree(0);
//    int numDevices;
//    cudaGetDeviceCount(&numDevices);
//    if (numDevices == 0)
//        throw std::runtime_error("#osc: no CUDA capable devices found!");
//    std::cout << "#osc: found " << numDevices << " CUDA devices" << std::endl;
//
//    // -------------------------------------------------------
//    // initialize optix
//    // -------------------------------------------------------
//    OPTIX_CHECK(optixInit());
//    std::cout << GDT_TERMINAL_GREEN
//        << "#osc: successfully initialized optix... yay!"
//        << GDT_TERMINAL_DEFAULT << std::endl;
//}
//
//static void context_log_cb(unsigned int level,
//    const char* tag,
//    const char* message,
//    void*)
//{
//    fprintf(stderr, "[%2d][%12s]: %s\n", (int)level, tag, message);
//}
//
///*! creates and configures a optix device context (in this simple
//  example, only for the primary GPU device) */
//void SampleRenderer::createContext()
//{
//    // for this sample, do everything on one device
//    const int deviceID = 0;
//    CUDA_CHECK(SetDevice(deviceID));
//    CUDA_CHECK(StreamCreate(&stream));
//
//    cudaGetDeviceProperties(&deviceProps, deviceID);
//    std::cout << "#osc: running on device: " << deviceProps.name << std::endl;
//
//    CUresult  cuRes = cuCtxGetCurrent(&cudaContext);
//    if (cuRes != CUDA_SUCCESS)
//        fprintf(stderr, "Error querying current context: error code %d\n", cuRes);
//
//    OPTIX_CHECK(optixDeviceContextCreate(cudaContext, 0, &optixContext));
//    OPTIX_CHECK(optixDeviceContextSetLogCallback
//    (optixContext, context_log_cb, nullptr, 4));
//}

enum Location {
	// CPU
	Host,
	// GPU
	Device,
	// MANAGED
	Auto
};
template<Location E>
class MemoryPointer
{
public:
	MemoryPointer(void* aPointer) : mPointer(aPointer) {}
	void* pointer() const { return mPointer; }
private:
	void* mPointer;
};



#define CUDA_CHECK(call)							                \
{									                                \
	cudaError_t rc = call;                                          \
	if (rc != cudaSuccess) {                                        \
		cudaError_t err =  rc; /*cudaGetLastError();*/              \
	}																\
}

#define CUO_INLINE inline
#define CUO_BEGIN_NAMESPACE namespace cuo {
#define CUO_END_NAMESPACE }
#define CUO_USE_NAMESPACE using namespace cuo;