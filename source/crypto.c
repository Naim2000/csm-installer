#include <stdio.h>

#include "crypto.h"

void GetTitleKey(tik* p_tik, aeskey out) {
	mbedtls_aes_context aes = {};
	aesiv iv = {};
	iv.titleid = p_tik->titleid;

	uint8_t commonKeyIndex = p_tik->reserved[0xb];
	if (commonKeyIndex > 0x02) {
		printf("Unknown common key index!? (0x%hhx)\n", commonKeyIndex);
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

	if (index > 0x02)
		return;

	GetTitleKey(p_tik, in);
	p_tik->reserved[0xb] = index;
	mbedtls_aes_setkey_enc(&aes, CommonKeys[index], 128);
	mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, sizeof(aeskey),
						  iv.full, in, p_tik->cipher_title_key);
}
