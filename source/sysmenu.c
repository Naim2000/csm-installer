#include "sysmenu.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ogc/es.h>
#include <ogc/isfs.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>
#include <mbedtls/aes.h>

#include "pad.h"
#include "fs.h"

void* memalign(size_t, size_t);

static const char u8_header[] = { 0x55, 0xAA, 0x38, 0x2D };
static const aeskey vwii_ckey = { 0x30, 0xbf, 0xc7, 0x6e, 0x7c, 0x19, 0xaf, 0xbb, 0x23, 0x16, 0x33, 0x30, 0xce, 0xd7, 0xc2, 0x8d };
static const aeskey wii_ckey  = { 0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7 };

static u64 __smNUSTID = 0x0000000100000002LL;
static aeskey __smTitleKey = {};
static int __hasPriiloader = 0;
static u32 __archiveCid = 0;
static size_t __archiveSize = 0; // stupid
static sha1 __archiveHash = {};
static char __archivePath[ISFS_MAXPATH];
static u16 __smVersion = 0;
static s8  __smRegion  = 0;
static s8  __smMajor   = 0;

/* from YAWM ModMii Edition
const u16 VersionList[] = {
//	J		U		E		K

	64,		33,		66,					// 1.0 | 0040, 0021, 0042, ---- "1", "2"
	128,	97,		130,				// 2.0 | 0080, 0061, 0082, ---- "3", "4"
					162,				// 2.1 | ----, ----, 00a2, ---- "5"
	192,	193,	194,				// 2.2 | 00c0, 00c1, 00c2, ---- "6"
	224,	225,	226,				// 3.0 | 00e0, 00e1, 00e2, ---- "7"
	256,	257,	258,				// 3.1 | 0100, 0101, 0102, ---- "8"
	288,	289,	290,				// 3.2 | 0120, 0121, 0122, ---- "9"
	352,	353,	354,	326,		// 3.3 | 0160, 0161, 0162, 0146 "10", "11"
	384,	385,	386, 				// 3.4 | 0180, 0181, 0182, ---- "12"
							390, 		// 3.5 | ----, ----, ----, 0186 "12"
	416,	417,	418,				// 4.0 | 01a0, 01a1, 01a2, ---- "13"
	448,	449,	450,	454, 		// 4.1 | 01c0, 01c1, 01c2, 01c6 "14"
	480,	481,	482,	486, 		// 4.2 | 01e0, 01e1, 01e2, 01e6 "15"
	512,	513, 	514,	518, 		// 4.3 | 0200, 0201, 0202, 0206 "16"
};
*/

static char _getSMVersionMajor(u16 rev) {
	switch (rev >> 4) {
		case 0x02:
		case 0x04:
			return '1';

		case 0x06:
		case 0x08:
		case 0x0a:
		case 0x0c:
			return '2';

		case 0x0e:
		case 0x10:
		case 0x12:
		case 0x14:
		case 0x16:
		case 0x18:
			return '3';

		case 0x1a:
		case 0x1c:
		case 0x1e:
		case 0x20:
		// vWii
		case 0x22:
		case 0x26:
		// Wii Mini
		case 0x120:
			return '4';
	}

	return 0;
}

static char _getSMRegion(u16 rev) {
	switch (rev & 0xf) {
		case 0:
			return 'J';

		case 1:
			return 'U';

		case 2:
			return 'E';

		case 6:
			return 'K';
	}

	return 0;
}

int sysmenu_process() {
	int ret;
	u32 size = 0;
	void* buffer = NULL;
	char sysmenu_filepath[ISFS_MAXPATH] = "/title/00000001/00000002/content/";

	ret = ES_GetStoredTMDSize(0x100000002LL, &size);
	if (ret < 0) {
		printf("ES_GetStoredTMDSize failed (%i)\n", ret);
		goto finish;
	}

	buffer = memalign(0x20, size);
	if (!buffer) {
		printf("Failed to allocate space for TMD...?\n");
		ret = -ENOMEM;
		goto finish;
	}

	ret = ES_GetStoredTMD(0x100000002LL, buffer, size);
	if (ret < 0) {
		printf("ES_GetTMDView failed (%i)\n", ret);
		goto finish;
	}

	tmd* sysmenu_tmd = SIGNATURE_PAYLOAD((signed_blob*)buffer);

	__smVersion = sysmenu_tmd->title_version;
//	printf("System menu version: %hu (%04hx)\n", __smVersion, __smVersion);
	if (__smVersion > 0x2000) {
		printf("Bad system menu version! (%hu/%04hx)\nInvalid or not vanilla.\n", __smVersion, __smVersion);
		ret = -EINVAL;
		goto finish;
	}

	__smRegion = _getSMRegion(__smVersion);
//	printf("System menu region (from rev number): %c\n", __smRegion);
	if (!__smRegion) {
		printf("Bad system menu version! (%hu/%04hx)\nUnable to identify region (what's the last hex digit?)\n", __smVersion, __smVersion);
		ret = -EINVAL;
		goto finish;
	}

	__smMajor = _getSMVersionMajor(__smVersion);
//	printf("System menu major version (from rev number): %c\n", __smMajor);
	if (!__smMajor) {
		printf("Bad system menu version! (%hu/%04hx)\nUnable to identify major revision.\n", __smVersion, __smVersion);
		ret = -EINVAL;
		goto finish;
	}

	for (int i = 0; i < sysmenu_tmd->num_contents; i++) {
		tmd_content* content = sysmenu_tmd->contents + i;

		if (content->type == 0x8001)
			continue;

		if (content->index == sysmenu_tmd->boot_index) { // how Priiloader installer does it
			sprintf(strrchr(sysmenu_filepath, '/'), "/%08x.app", content->cid);
			*(strrchr(sysmenu_filepath, '/') + 1) = '1'; // Also how Priiloader installer does it. sort of
			ret = NAND_GetFileSize(sysmenu_filepath, NULL);

			__hasPriiloader = (ret >= 0);
		}
		else {
			char header[4] ATTRIBUTE_ALIGN(0x20) = {};

			sprintf(strrchr(sysmenu_filepath, '/'), "/%08x.app", content->cid);
			ret = NAND_Read(sysmenu_filepath, header, 4, NULL);
			if (ret < 0) {
				printf("Failed to read %s (%i)\n", sysmenu_filepath, ret);
				continue;
			}

			if (memcmp(header, u8_header, sizeof(u8_header)) == 0) {
				__archiveCid = content->cid;
				__archiveSize = (size_t)content->size;
				strcpy(__archivePath, sysmenu_filepath);
				memcpy(__archiveHash, content->hash, sizeof(sha1));
				continue;
			}
		}
	}

	if(!__archiveCid) {
		printf("Failed to identify system menu archive!");
		ret = -ENOENT;
		goto finish;
	}

	if (size < STD_SIGNED_TIK_SIZE) {
		free(buffer);
		buffer = memalign(0x20, STD_SIGNED_TIK_SIZE);
		if (!buffer) {
			printf("Failed to allocate space for ticket...?\n");
			ret = -ENOMEM;
			goto finish;
		}
	}
	ret = NAND_Read("/ticket/00000001/00000002.tik", buffer, STD_SIGNED_TIK_SIZE, NULL);
	if (ret < 0) {
		printf("Failed to read system menu ticket (%i)\n", ret);
		goto finish;
	}

	tik* sysmenu_tik = SIGNATURE_PAYLOAD((signed_blob*) buffer);

	aeskey iv = {};
	mbedtls_aes_context tkey = {};
	memcpy(iv, &sysmenu_tik->titleid, sizeof(u64));
	switch (sysmenu_tik->reserved[0xb]) {
		case 0:
			mbedtls_aes_setkey_dec(&tkey, wii_ckey, sizeof(aeskey) * 8);
			break;
		case 2:
			__smNUSTID |= 0x6LL << 32;
			mbedtls_aes_setkey_dec(&tkey, vwii_ckey, sizeof(aeskey) * 8);
			break;

		default:
			printf("Unknown common key index?\n");
			ret = -EINVAL;
			goto finish;
	}
	mbedtls_aes_crypt_cbc(&tkey, MBEDTLS_AES_DECRYPT, sizeof(aeskey), iv, sysmenu_tik->cipher_title_key, __smTitleKey);

finish:
	free(buffer);
	return ret;
}

u64 getSmNUSTitleID() {
	return __smNUSTID;
}

bool hasPriiloader() {
	return __hasPriiloader > 0;
}

u32 getArchiveCid() {
	return __archiveCid;
}

size_t getArchiveSize() {
	return __archiveSize;
}

const char* getArchivePath() {
	return __archivePath;
}

u8* getSmTitleKey() {
	return __smTitleKey;
}

u16 getSmVersion() {
	return __smVersion;
}

u8* getArchiveHash() {
	return __archiveHash;
}

char getSmRegion() {
	return __smRegion;
}

char getSmVersionMajor() {
	return __smMajor;
}
