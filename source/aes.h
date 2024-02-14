/*-------------------------------------------------------------

aes.h -- AES Engine

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


#ifndef __AES_H___
#define __AES_H___

#if defined(HW_RVL)
#include <stdint.h>

/* This is needed because IOS isn't smart enough to do it for us. */
#define AES_BLOCK_SIZE 0x10000

#ifdef __cplusplus
	extern "C" {
#endif /* __cplusplus */

int AES_Init(void);
void AES_Close(void);
int AES_Decrypt(const void* key, void* iv, const void* in_data, void* out_data, uint32_t data_size);
int AES_Encrypt(const void* key, void* iv, const void* in_data, void* out_data, uint32_t data_size);

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif
#endif
