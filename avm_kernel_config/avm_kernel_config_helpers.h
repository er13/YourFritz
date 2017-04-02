// vi: set tabstop=4 syntax=c :
#ifndef AVM_KERNEL_CONFIG_HELPERS_H
#define AVM_KERNEL_CONFIG_HELPERS_H

#include <stdbool.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#include <sys/mman.h>

#ifdef USE_STRIPPED_AVM_KERNEL_CONFIG_H
#include "avm_kernel_config.h"
#else
#include "linux/include/uapi/linux/avm_kernel_config.h"
#endif

struct memoryMappedFile
{
	const char *		fileName;
	const char *		fileDescription;
	int					fileDescriptor;
	struct stat			fileStat;
	void *				fileBuffer;
	bool				fileMapped;
};

bool openMemoryMappedFile(struct memoryMappedFile *file, const char *fileName, const char *fileDescription, int openFlags, int prot, int flags);
void closeMemoryMappedFile(struct memoryMappedFile *file);
bool isConsistentConfigArea(struct _avm_kernel_config * *configArea, size_t configSize, bool *swapNeeded);
void swapEndianess(bool needed, uint32_t *ptr);
uint32_t determineConfigAreaKernelSegment(uint32_t targetAddressSpacePtr);

#endif
