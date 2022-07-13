
#define CL_TARGET_OPENCL_VERSION 210
#include <CL/opencl.h>
#include <cassert>
#include <iostream>

extern unsigned char KernelSpirV[];
extern unsigned int KernelSpirVLength;


extern "C" {
  void* runOpenCLKernel(void *NativeEventDep, uintptr_t *NativeHandles, int NumHandles, unsigned Blocks, unsigned Threads, unsigned Arg1, void *Arg2, void *Arg3);
}

static cl_kernel Kernel = 0;
static cl_program Program = 0;

void* runOpenCLKernel(void *NativeEventDep, uintptr_t *NativeHandles, int NumHandles, unsigned Blocks, unsigned Threads, unsigned Arg1, void *Arg2, void *Arg3) {
  assert (NumHandles == 4);
  int Err = 0;
  //cl_platform_id Plat = (cl_platform_id)NativeHandles[0];
  cl_device_id Dev = (cl_device_id)NativeHandles[1];
  cl_context Ctx = (cl_context)NativeHandles[2];
  cl_command_queue CQ = (cl_command_queue)NativeHandles[3];

  cl_event DepEv = (cl_event)NativeEventDep;

  if (Program == 0) {
    Program = clCreateProgramWithIL(Ctx, KernelSpirV, KernelSpirVLength, &Err);
    assert (Err == CL_SUCCESS);
    assert (Program);

    Err = clBuildProgram(Program, 1, &Dev, NULL, NULL, NULL);
    if (Err != CL_SUCCESS) {
      std::cout << "build failed, build log:\n";
      size_t LogSize = 0;
      Err = clGetProgramBuildInfo(Program, Dev, CL_PROGRAM_BUILD_LOG, 0, NULL, &LogSize);
      assert (Err == CL_SUCCESS);
      char *BuildLog = new char[LogSize];
      Err = clGetProgramBuildInfo(Program, Dev, CL_PROGRAM_BUILD_LOG, LogSize, BuildLog, NULL);
      assert (Err == CL_SUCCESS);
      std::cout << BuildLog << "\n";
      delete [] BuildLog;
      return NULL;
    }

    Kernel = clCreateKernel(Program, "binomial_options.1", &Err);
    assert (Err == CL_SUCCESS);

    Err = clSetKernelArg(Kernel, 0, sizeof(int), &Arg1);
    assert (Err == CL_SUCCESS);
    Err = clSetKernelArgSVMPointer(Kernel, 1, Arg2);
    assert (Err == CL_SUCCESS);
    Err = clSetKernelArgSVMPointer(Kernel, 2, Arg3);
    assert (Err == CL_SUCCESS);
  }

  size_t Goffs0[3] = { 0, 0, 0 };
  size_t GWS[3] = { Blocks*Threads, 0, 0 };
  size_t LWS[3] = { Threads, 0, 0 };
  cl_event RetEvent = 0;
  Err = clEnqueueNDRangeKernel(CQ, Kernel, 1, Goffs0, GWS, LWS, 1, &DepEv, &RetEvent);
  assert (Err == CL_SUCCESS);
  assert (RetEvent != 0);
  return (void*)RetEvent;
}
