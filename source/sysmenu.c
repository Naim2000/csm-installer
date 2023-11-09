#include "sysmenu.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ogc/es.h>
#include <ogc/isfs.h>

void* memalign(size_t, size_t);

static int __hasPriiloader = ~0;
static u32 __archiveCid = 0;

int sysmenu_process() {
	int ret;
	u32 size = 0;

	ret = ES_GetStoredTMDSize(0x100000002LL, &size);
	if (ret < 0) {
		printf("ES_GetStoredTMDSize failed (%d)\n", ret);
		return ret;
	}

	void* buffer = memalign(0x20, size);
	if (!buffer) {
		printf("Failed to allocate space for TMD...?\n");
		return ret;
	}

	ret = ES_GetStoredTMD(0x100000002LL, buffer, size);
	if (ret < 0) {
		printf("ES_GetTMDView failed (%d)\n", ret);
		return ret;
	}

	tmd* sysmenu_tmd = SIGNATURE_PAYLOAD((signed_blob*)buffer);

	for (int i = 0; i < sysmenu_tmd->num_contents; i++) {
		tmd_content* content = sysmenu_tmd->contents + i;

		if (content->type == 0x8001) continue;

		if (content->index == sysmenu_tmd->boot_index) { // how Priiloader installer does it
			sprintf(strrchr(sysmenu_filepath, '/'), "/%08x.app", content->cid | 0x10000000);
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

	free(buffer);
	return 0;
}

bool hasPriiloader() {
	return __hasPriiloader > 0;
}

u32 getArchiveCid() {
	return __archiveCid;
}
