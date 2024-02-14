#include <stdint.h>
#include <ogc/sha.h>
#include <ogc/es.h>
#include <mbedtls/aes.h>

#include "aes.h"

typedef union {
	int16_t index;
	int64_t titleid;

	int64_t part[2];

	uint8_t full[0x10];
} aesiv;

extern const aeskey CommonKeys[3];

void GetTitleKey(tik*, aeskey);
void ChangeCommonKey(tik*, uint8_t);
