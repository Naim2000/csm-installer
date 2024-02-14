#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "crypto.h"
#include "malloc.h"

const aeskey CommonKeys[3] = {
	/* Standard common key */	{ 0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7 },
	/*  Korean common key  */	{ 0x63, 0xb8, 0x2b, 0xb4, 0xf4, 0x61, 0x4e, 0x2e, 0x13, 0xf2, 0xfe, 0xfb, 0xba, 0x4c, 0x9b, 0x7e },
	/*   vWii common key   */	{ 0x30, 0xbf, 0xc7, 0x6e, 0x7c, 0x19, 0xaf, 0xbb, 0x23, 0x16, 0x33, 0x30, 0xce, 0xd7, 0xc2, 0x8d },
};

void GetTitleKey(tik* p_tik, aeskey out) {
	mbedtls_aes_context aes = {};
	aesiv iv = {};
	iv.titleid = p_tik->titleid;

	uint8_t commonKeyIndex = p_tik->reserved[0xb];
	if (commonKeyIndex > 0x02) {
		printf("\t\x1b[30;1mUnknown common key index!? (0x%hhx)\x1b[39m\n", commonKeyIndex);
		commonKeyIndex = 0;
	}
	mbedtls_aes_setkey_dec(&aes, CommonKeys[commonKeyIndex], 128);
	mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, sizeof(aeskey),
						  iv.full, p_tik->cipher_title_key, out);
}

void ChangeCommonKey(tik* p_tik, uint8_t index) {
	mbedtls_aes_context aes = {};
	aeskey in;
	aesiv iv = {};
	iv.titleid = p_tik->titleid;

	if (index > 0x02) return;

	GetTitleKey(p_tik, in);
	p_tik->reserved[0xb] = index;
	mbedtls_aes_setkey_enc(&aes, CommonKeys[index], 128);
	mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, sizeof(aeskey),
						  iv.full, in, p_tik->cipher_title_key);
}
