#include "sysmenu.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ogc/es.h>
#include <ogc/isfs.h>
#include <wiiuse/wpad.h>
#include "pad.h"

void* memalign(size_t, size_t);

static int __hasPriiloader = 0;
static u32 __archiveCid = 0;
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

	ret = ES_GetStoredTMDSize(0x100000002LL, &size);
	if (ret < 0) {
		printf("ES_GetStoredTMDSize failed (%d)\n", ret);
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
		printf("ES_GetTMDView failed (%d)\n", ret);
		goto finish;
	}

	tmd* sysmenu_tmd = SIGNATURE_PAYLOAD((signed_blob*)buffer);

	__smVersion = sysmenu_tmd->title_version;
	printf("System menu version: %hu (%04hx)\n", __smVersion, __smVersion);
	if (__smVersion > 0x2000) {
		printf("Bad system menu version! (%hu/%04hx)\nInvalid or not vanilla.\n", __smVersion, __smVersion);
		ret = -EINVAL;
		goto finish;
	}

	__smRegion = _getSMRegion(__smVersion);
	printf("System menu region (from rev number): %c\n", __smRegion);
	if (!__smRegion) {
		printf("Bad system menu version! (%hu/%04hx)\nUnable to identify region (what's the last hex digit?)\n", __smVersion, __smVersion);
		ret = -EINVAL;
		goto finish;
	}

	__smMajor = _getSMVersionMajor(__smVersion);
	printf("System menu major version (from rev number): %c\n", __smMajor);
	if (!__smMajor) {
		printf("Bad system menu version! (%hu/%04hx)\nUnable to identify major revision.\n", __smVersion, __smVersion);
		ret = -EINVAL;
		goto finish;
	}


	for (int i = 0; i < sysmenu_tmd->num_contents; i++) {
		tmd_content* content = sysmenu_tmd->contents + i;

		if (content->type == 0x8001) continue;

		if (content->index == sysmenu_tmd->boot_index) { // how Priiloader installer does it
			sprintf(strrchr(sysmenu_filepath, '/'), "/%08x.app", content->cid);
			*(strrchr(sysmenu_filepath, '/') + 1) = '1'; // Also how Priiloader installer does it. sort of
			ret = ISFS_Open(sysmenu_filepath, 0);
			if (ret > 0) ISFS_Close(ret);

			__hasPriiloader = ret > 0 || ret == -102;
		}
		else {
			[[gnu::aligned(0x20)]] u32 header = 0;

			sprintf(strrchr(sysmenu_filepath, '/'), "/%08x.app", content->cid);
			ret = ISFS_Open(sysmenu_filepath, ISFS_OPEN_READ);
			if (ret < 0) {
				printf("Failed to open %s (%d)\n", sysmenu_filepath, ret);
				continue;
			}

			ISFS_Read(ret, &header, sizeof(header));
			ISFS_Close(ret);

			if (header == 0x55AA382D) {
				__archiveCid = content->cid;
				printf("Found the archive file! (%08x)\n", content->cid);
				continue;
			}
		}
	}

finish:
	free(buffer);
	printf("Press HOME to continue.\n");
	wait_button(WPAD_BUTTON_HOME);
	return ret;
}

bool hasPriiloader() {
	return __hasPriiloader > 0;
}

u32 getArchiveCid() {
	return __archiveCid;
}

u16 getSMVersion() {
	return __smVersion;
}

char getSmRegion() {
	return __smRegion;
}

char getSmVersionMajor() {
	return __smMajor;
}
