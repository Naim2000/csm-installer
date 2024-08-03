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
#include "crypto.h"
#include "malloc.h"
#include "sysmenu.h"
#include "theme.h"

static const char u8_header[] = { 0x55, 0xAA, 0x38, 0x2D };

struct sysmenu sysmenu[1];

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

static char _getSMVersionMajor(uint16_t rev) {
	if (!rev)
		return 0;

	switch (rev >> 5) {
		case 0x01:
		case 0x02:
			return '1';

		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
			return '2';

		case 0x07:
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0B:
		case 0x0C:
			return '3';

		case 0x0D:
		case 0x0E:
		case 0x0F:
		case 0x10:
		// vWii
		case 0x11:
		case 0x12:
		// Wii Mini
		case 0x90:
			return '4';
	}

	return 0;
}

static char _getSMRegion(uint16_t rev) {
	if (!rev)
		return 0;

	switch (rev & 0x1F) {
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
	uint32_t size = 0;
	signed_blob* buffer = NULL;
	char filepath[0x80] __aligned(0x20) = "/title/00000001/00000002/content/";

	ret = ES_GetStoredTMDSize(0x100000002LL, &size);
	if (ret < 0) {
		printf("ES_GetStoredTMDSize failed (%i)\n", ret);
		goto finish;
	}

	buffer = memalign32(MAX(size, STD_SIGNED_TIK_SIZE));
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

	tmd* p_tmd = SIGNATURE_PAYLOAD(buffer);

	bool isvWii = (bool)p_tmd->vwii_title;
	uint16_t rev = sysmenu->version = p_tmd->title_version;
	if (rev > 0x2000) {
		printf("Bad system menu version! (%hu/%04hx)\nInvalid or not vanilla.\n", rev, rev);
		ret = -EINVAL;
		goto finish;
	}

	if (!(sysmenu->region = _getSMRegion(rev))) {
		printf("Bad system menu version! (%hu/%04hx)\nUnable to identify region (what are the last 4 bits?)\n", rev, rev);
		ret = -EINVAL;
		goto finish;
	}

	if (!(sysmenu->versionMajor = _getSMVersionMajor(rev))) {
		printf("Bad system menu version! (%hu/%04hx)\nUnable to identify major revision.\n", rev, rev);
		ret = -EINVAL;
		goto finish;
	}

	for (int i = 0; i < p_tmd->num_contents; i++) {
		tmd_content* content = p_tmd->contents + i;
		if (content->type & 0x8000)
			continue;

		/*
			STACK DUMP:
			800fe468 <memmove+376>			stb r6,0(r7)
			8010f968 <__ssprint_r+104>		bl 0x800fe350 <memmove>
			80008488 <sysmenu_process+560>	bl 0x800f7038 <sprintf>

			"why is r7 0..?"
			*makes filepath 2x bigger
			"Failed to open <garbage memory>0000001/00000002/content/00000097.app (-6)"
			*add __aligned(0x20)
			*works
			*ok?
		*/
		sprintf(strrchr(filepath, '/'), "/%08x.app", content->cid);

		if (content->index == p_tmd->boot_index) { // how Priiloader installer does it
			strrchr(filepath, '/')[1] = '1';
			sysmenu->hasPriiloader = (NAND_GetFileSize(filepath, NULL) >= 0);
		}
		else {
			char header[4] ATTRIBUTE_ALIGN(0x20) = {};
			ret = NAND_Read(filepath, header, 4, NULL);
			if (ret < 0) {
				printf("Failed to read %s (%i)\n", filepath, ret);
				continue;
			}

			if (memcmp(header, u8_header, sizeof(u8_header)) == 0)
				sysmenu->archive = *content;
		}
	}

	if (!sysmenu->archive.cid) {
		puts("Failed to identify system menu archive!");
		ret = -ENOENT;
		goto finish;
	}

	ret = NAND_Read("/ticket/00000001/00000002.tik", buffer, STD_SIGNED_TIK_SIZE, NULL);
	if (ret < 0) {
		printf("Failed to read system menu ticket (%i)\n", ret);
		goto finish;
	}

	tik* p_tik = SIGNATURE_PAYLOAD(buffer);
	if ((sysmenu->isvWii = isvWii)) {
		sysmenu->platform = (ThemeBase)vWii;
	}
	else if ((rev & 0xFFE0) == 0x1200) {
		sysmenu->platform = (ThemeBase)Mini;
	}
	else {
		sysmenu->platform = (ThemeBase)Wii;
	}

	GetTitleKey(p_tik, sysmenu->titlekey);

finish:
	free(buffer);
	return ret;
}
