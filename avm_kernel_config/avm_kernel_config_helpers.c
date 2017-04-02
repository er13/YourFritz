// vi: set tabstop=4 syntax=c :
/***********************************************************************
 *                                                                     *
 *                                                                     *
 * Copyright (C) 2016 P.Hämmerlein (http://www.yourfritz.de)           *
 * Modified by Eugene Rudoy (https://github.com/er13)                  *
 *                                                                     *
 * This program is free software; you can redistribute it and/or       *
 * modify it under the terms of the GNU General Public License         *
 * as published by the Free Software Foundation; either version 2      *
 * of the License, or (at your option) any later version.              *
 *                                                                     *
 * This program is distributed in the hope that it will be useful,     *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of      *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the       *
 * GNU General Public License for more details.                        *
 *                                                                     *
 * You should have received a copy of the GNU General Public License   *
 * along with this program, please look for the file COPYING.          *
 *                                                                     *
 ***********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "avm_kernel_config_helpers.h"

bool openMemoryMappedFile(struct memoryMappedFile *file, const char *fileName, const char *fileDescription, int openFlags, int prot, int flags)
{
	bool			result = false;

	file->fileMapped = false;
	file->fileBuffer = NULL;
	file->fileName = fileName;
	file->fileDescription = fileDescription;

	if ((file->fileDescriptor = open(file->fileName, openFlags)) != -1)
	{
		if (fstat(file->fileDescriptor, &file->fileStat) != -1)
		{
			if ((file->fileBuffer = (void *) mmap(NULL, file->fileStat.st_size, prot, flags, file->fileDescriptor, 0)) != MAP_FAILED)
			{
				file->fileMapped = true;
				result = true;
			}
			else fprintf(stderr, "Error %d mapping %u bytes of %s file '%s' to memory.\n", errno, (int) file->fileStat.st_size, file->fileDescription, file->fileName);
		}
		else fprintf(stderr, "Error %d getting file stats for '%s'.\n", errno, file->fileName);

		if (result == false)
		{
			close(file->fileDescriptor);
			file->fileDescriptor = -1;
		}
	}
	else fprintf(stderr, "Error %d opening %s file '%s'.\n", errno, file->fileDescription, file->fileName);

	return result;
}

void closeMemoryMappedFile(struct memoryMappedFile *file)
{

	if (file->fileMapped)
	{
		munmap(file->fileBuffer, file->fileStat.st_size);
		file->fileBuffer = NULL;
		file->fileMapped = false;
	}

	if (file->fileDescriptor != -1)
	{
		close(file->fileDescriptor);
		file->fileDescriptor = -1;
	}

}

bool isConsistentConfigArea(struct _avm_kernel_config * *configArea, size_t configSize, bool *swapNeeded, uint32_t *derived_avm_kernel_config_tags_last)
{
	uint32_t *					arrayStart = NULL;
	uint32_t *					arrayEnd = NULL;
	uint32_t *					ptr = NULL;
	uint32_t *					base = NULL;
	uint32_t					kernelSegmentStart;
	uint32_t					lastTag;
	uint32_t					ptrValue;
	struct _avm_kernel_config *	entry;
	bool						assumeSwapped = false;

	//	- a 32-bit value with more than one byte containing a non-zero value
	//	  should be a pointer in the config area
	//	- a value with only one non-zero byte is usually the tag, tags are
	//	  'enums' and have to be below or equal avm_kernel_config_tags_last
	//	- values without any bit set are expected to be alignments or end of
	//	  array markers
	//	- we'll stop at the second 'end of array' marker, assuming we've
	//	  reached the end of 'struct _avm_kernel_config' array, the tag at
	//	  this array entry should be equal to avm_kernel_config_tags_last
	//	- limit search to first 16 KB (4096 * sizeof(uint32_t)), if the whole
	//	  area is empty

	ptr = (uint32_t *) configArea;

	while (ptr <= ((uint32_t *) configArea) + (4096 * sizeof(uint32_t)))
	{
		if (*ptr != 0)
		{
			if (base == NULL) {
				base = ptr;       // 1st non-zero word is base (pointer)
			} else if (arrayStart == NULL) {
				arrayStart = ptr; // 2nd non-zero word is arrayStart
			} /* else ... */          // all other non-zero words are the content of the config area array => ignore them
		}
		else
		{
			if (base == NULL) {
				// we don't expect to find zero word before base is set (this actually means the 1st word must be non-zero and it's the base pointer)
				return false; // no pointer, no config area array => inconsistent config area
			} else if (arrayStart != NULL) {
				// 1st zero word after arrayStart => this is arrayEnd
				// or to be more precise zero word it the config-pointer of the entry with tag avm_kernel_config_tags_last
				arrayEnd = ptr + 1; // thus +1, arrayEnd is exclusive
				break;
			}
		}
		ptr++;
	}

	// if we didn't find one of our pointers, something went wrong
	if (base == NULL || arrayStart == NULL || arrayEnd == NULL) return false;

	// check avm_kernel_config_tags_last entry first
	entry = (struct _avm_kernel_config *) arrayEnd - 1;
	lastTag = entry->tag;

	// guess if endianness swap is required
#ifdef USE_STRIPPED_AVM_KERNEL_CONFIG_H
	// stripped "avm_kernel_config.h" intentionally doesn't provide avm_kernel_config_tags_last symbol
	#define MAX_PLAUSIBLE_AVM_KERNEL_CONFIG_TAGS_ENUM (0x000001FF)
	assumeSwapped = (lastTag <= MAX_PLAUSIBLE_AVM_KERNEL_CONFIG_TAGS_ENUM ? false : true);
	swapEndianess(assumeSwapped, &lastTag);
	if (!(avm_kernel_config_tags_undef < lastTag && lastTag <= MAX_PLAUSIBLE_AVM_KERNEL_CONFIG_TAGS_ENUM))
		return false;
#else
	assumeSwapped = (lastTag <= avm_kernel_config_tags_last ? false : true);
	swapEndianess(assumeSwapped, &lastTag);
	if (lastTag != avm_kernel_config_tags_last)
		return false;
#endif

	// check other tags
	for (entry = (struct _avm_kernel_config *) arrayStart; entry->config != NULL; entry++)
	{
		uint32_t tag = entry->tag;
		swapEndianess(assumeSwapped, &tag);
		// invalid value means, our assumption was wrong
		if (!(avm_kernel_config_tags_undef < tag && tag <= lastTag)) /* that lastTag is in range has been validated before */
			return false;
	}

	// compute the start of the kernel "segment" config area is located within (target address space)
	ptrValue = *base;
	swapEndianess(assumeSwapped, &ptrValue);
	kernelSegmentStart = ptrValue & 0xFFFFF000;

	// first value has to point to the array
	if ((ptrValue - kernelSegmentStart) != ((uint32_t) arrayStart - (uint32_t) configArea))
		return false;

	// check each entry->config pointer, if its value is in range
	for (entry = (struct _avm_kernel_config *) arrayStart; entry->config != NULL; entry++)
	{
		ptrValue = (uint32_t) entry->config;
		swapEndianess(assumeSwapped, &ptrValue);

		if (ptrValue <= kernelSegmentStart) return false; // points before, impossible
		if (ptrValue - kernelSegmentStart > configSize) return false; // points after
	}

	// we may be sure here, that the endianess was detected successful
	if (swapNeeded)
		*swapNeeded = assumeSwapped;
	if (derived_avm_kernel_config_tags_last)
		*derived_avm_kernel_config_tags_last = lastTag;
	return true;
}

void swapEndianess(bool needed, uint32_t *ptr)
{

	if (!needed) return;
	*ptr = 	(*ptr & 0x000000FF) << 24 | \
			(*ptr & 0x0000FF00) << 8 | \
			(*ptr & 0x00FF0000) >> 8 | \
			(*ptr & 0xFF000000) >> 24;

}

