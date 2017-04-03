// vi: set tabstop=4 syntax=c :
#ifndef LIB_AVM_KERNEL_CONFIG_H
#define LIB_AVM_KERNEL_CONFIG_H

#include <stdbool.h>
#include <inttypes.h>

#ifdef USE_STRIPPED_AVM_KERNEL_CONFIG_H
#include "avm_kernel_config.h"
#else
#include "linux/include/uapi/linux/avm_kernel_config.h"
#endif

bool isConsistentConfigArea(void *configArea, size_t configSize, bool *swapNeeded);
bool relocateConfigArea(void *configArea, size_t configSize);

void swapEndianess(bool needed, uint32_t *ptr);

uint32_t determineConfigAreaKernelSegment(uint32_t targetAddressSpacePtr);
void* targetPtr2HostPtr(uint32_t targetAddressSpacePtr, uint32_t targetAddressSpaceBasePtr, void* hostAddressSpaceBasePtr);

#endif
