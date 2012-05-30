/* tce_common.h - common code for the different TCE/TTA device drivers.

   Copyright (c) 2012 Pekka Jääskeläinen / Tampere University of Technology
   
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/
#ifndef POCL_TCE_COMMON_H
#define POCL_TCE_COMMON_H


#include "bufalloc.h"

#ifdef __cplusplus

#include <string>

namespace TTAMachine {
  class AddressSpace;
  class Machine;
}

namespace TTAProgram {
  class Program;
}

class TCEDevice {
 public:
  TCEDevice(cl_device_id dev, const char* adfName);
  virtual ~TCEDevice();

  void initMemoryManagement(const TTAMachine::Machine& mach);

  /* Block read/write (no byteswaps). */
  virtual void copyHostToDevice
    (const void *host_ptr, uint32_t dest_addr, size_t count) = 0;

  virtual void copyDeviceToHost
    (uint32_t src_addr, const void *host_ptr, size_t count) = 0;

  virtual void loadProgramToDevice(const std::string& asmFileName) = 0;
  /* Restarts the device to start the program from the beginning. */
  virtual void restartProgram() = 0;

  /* Write/read word with a host->device / device->host byteswap. */
  virtual void writeWordToDevice(uint32_t dest_addr, uint32_t word);
  virtual uint32_t readWordFromDevice(uint32_t addr);

  /* Read the device time stamp for the profiling queue. */
  virtual uint64_t timeStamp() = 0;

  /* Finds the global data memory addresses needed for device->host
     communication from the loaded program. */
  virtual void findDataMemoryAddresses();

  /* Initializes the global communication structures in the device's
     global memory. Should be called once after loading the program to
     the device.*/
  virtual void initDataMemory();

  /* The bufalloc memory regions for device memory allocation book 
     keeping. */
  struct memory_region local_mem;
  struct memory_region global_mem;
  
  TTAMachine::AddressSpace *local_as;
  TTAMachine::AddressSpace *global_as;
  std::string machine_file;
  
  cl_device_id parent;

  bool needsByteSwap;

  const TTAProgram::Program* currentProgram;

  uint32_t commandQueueAddr;

};

#endif

/* The address space ids in the ADFs. */
#define TTA_ASID_PRIVATE  0
#define TTA_ASID_GLOBAL   3
#define TTA_ASID_LOCAL    4
#define TTA_ASID_CONSTANT 5

#define TTA_UNALLOCATED_LOCAL_SPACE (16*1024)
/* The space to preserve for the command queue etc. in the
   device global memory. The structures start from 0, the
   buffer storage starts after them. TODO: check from the
   symbol table of the produced program. */
#define TTA_UNALLOCATED_GLOBAL_SPACE (16*1024)

#ifdef __cplusplus
extern "C" {
#endif

void *
pocl_tce_malloc (void *device_data, cl_mem_flags flags,
                 size_t size, void *host_ptr);

void
pocl_tce_write (void *data, const void *host_ptr, void *device_ptr, 
                size_t cb);

void
pocl_tce_read (void *data, void *host_ptr, const void *device_ptr, 
               size_t cb);

void *
pocl_tce_create_sub_buffer (void *device_data, void* buffer, size_t origin, 
                            size_t size);


chunk_info_t*
pocl_tce_malloc_local (void *device_data, size_t size);

void
pocl_tce_free (void *data, cl_mem_flags flags, void *ptr);

void
pocl_tce_run 
(void *data, 
 _cl_command_node* cmd);

void *
pocl_tce_map_mem (void *data, void *buf_ptr, 
                     size_t offset, size_t size,
                  void *host_ptr);

#ifdef __cplusplus
}
#endif

#endif
