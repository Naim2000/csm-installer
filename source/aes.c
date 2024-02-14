/*-------------------------------------------------------------

aes.c -- AES Engine

Copyright (C) 2022
GaryOderNichts 
Joris 'DacoTaco' Vermeylen info@dacotaco.com

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

"iv with no const"
	- thepikachugamer

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/
#if defined(HW_RVL)

#include <string.h>
#include <errno.h>
#include <ogc/ipc.h>

#include "aes.h"

#define AES_HEAPSIZE 0x400

enum {
//	AES_IOCTLV_COPY		= 0 /* !? */
	AES_IOCTLV_ENCRYPT	= 2,
	AES_IOCTLV_DECRYPT	= 3,
};

static int __aes_fd = -1;
static int __aes_hid = -1;

static int AES_ExecuteCommand(int command, const void* key, void* iv, const void* in, void* out, uint32_t size)
{
	if(!__builtin_is_aligned(in, 0x10) || !__builtin_is_aligned(out, 0x10))
		return -EFAULT;

	if (size & 0x0F)
		return -EINVAL;

	ioctlv* params = (ioctlv*)iosAlloc(__aes_hid, sizeof(ioctlv) * 4);
	if (!params)
		return -1;

	int ret = -1;
	for (uint32_t i = 0; i < size; i += AES_BLOCK_SIZE) {
		uint32_t len = (size - i > AES_BLOCK_SIZE)
			? AES_BLOCK_SIZE
			: size - i;

		params[0].data	= (void*) in + i;
		params[0].len	= len;
		params[1].data	= (void*) key;
		params[1].len	= 0x10;

		params[2].data	= out + i;
		params[2].len	= len;
		params[3].data	= iv;
		params[3].len	= 0x10;

		ret = IOS_Ioctlv(__aes_fd, command, 2, 2, params);
		if (ret < 0)
			break;
	}

	iosFree(__aes_hid, params);
	return ret;
}

s32 AES_Init(void)
{
	if (__aes_fd >= 0)
		return 0;

	__aes_fd = IOS_Open("/dev/aes", 0);
	if (__aes_fd < 0)
		return __aes_fd;

	//only create heap if it wasn't created yet. 
	//its never disposed, so only create once.
	if(__aes_hid < 0)
		__aes_hid = iosCreateHeap(AES_HEAPSIZE);
	
	if (__aes_hid < 0) {
		AES_Close();
		return __aes_hid;
	}

	return 0;
}

void AES_Close(void)
{
	if (__aes_fd < 0)
		return;

	IOS_Close(__aes_fd);
	__aes_fd = -1;
}

int AES_Encrypt(const void* key, void* iv, const void* in_data, void* out_data, u32 data_size)
{
	return AES_ExecuteCommand(AES_IOCTLV_ENCRYPT, key, iv, in_data, out_data, data_size);
}

int AES_Decrypt(const void* key, void* iv, const void* in_data, void* out_data, u32 data_size)
{
	return AES_ExecuteCommand(AES_IOCTLV_DECRYPT, key, iv, in_data, out_data, data_size);
}

#endif
