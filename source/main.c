#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <gccore.h>
#include <ogc/es.h>
#include <wiiuse/wpad.h>
#include "libpatcher/libpatcher.h"

#include "video.h"
#include "malloc.h"
#include "pad.h"
#include "fs.h"
#include "fatMounter.h"
#include "directory.h"
#include "sysmenu.h"
#include "wad.h"
#include "network.h"

__weak_symbol
void OSReport(const char* fmt, ...) {}

extern void __exception_setreload(int);

bool isWADFile(const char* name) {
	return hasFileExtension(name, "wad");
}

bool isPPCTitle(uint64_t titleID) {
	return (titleID >> 32) != 0x1 || (titleID & ~0) == 0x2;
}

int selectWADFile(const char* filepath, void* userp) {
	int ret;

	wad_t* wad = wadInit(filepath);
	if (!wad) {
		puts("wadInit() failed");
		wait_button(0);
		return 0;
	}

	fseek(wad->fp, 0, SEEK_END);
	size_t fsize = ftell(wad->fp);
	rewind(wad->fp);

	printf("\nFile size: %.2fMB (0x%x)\n\n", fsize / 1048576.0f , fsize);

	printf(
		"Title ID : %016llx\n"
		"Revision : %hu (0x%04hx)\n"
		"IOS ver  : IOS%i\n\n", wad->titleID, wad->titleVer, wad->titleVer, wad->titleIOS);
/*
	printf(
		"Certificates size: 0x%x (%u)\n"
		"CRL size: 0x%x (%u)\n"
		"Ticket size: 0x%x (%u)\n"
		"TMD size: 0x%x (%u)\n\n", wad->header.certsSize,  wad->header.certsSize,
		wad->header.crlSize, wad->header.crlSize,
		wad->header.tikSize, wad->header.tikSize,
		wad->header.tmdSize, wad->header.tmdSize);

		printf("Contents count: %hu\n\n", wad->contentsCount);
*/
	int option = QuickActionMenu(7, (const char* []){
		"Install WAD",
		"Launch title",
		"Uninstall WAD",
		"Uninstall WAD (Remove TMD)",
		"Uninstall WAD (Remove TMD & Ticket(s))",
		"Uninstall WAD (Remove TMD & Console ticket(s))",
		"Unpack WAD"
	});
	if (option) {
		switch (option) {
			case 1: {
				ret = wadInstall(wad);
				if (ret < 0) printf("wadInstall() failed (%i)\n%s\n", ret, wad_strerror(ret));
			} break;

			case 2: {
				uint64_t titleID = wad->titleID;
				wadFree(wad);
				wad = NULL;

				if (!isPPCTitle(titleID)) {
					puts("Not launching an IOS.");
					break;
				}

				uint32_t tmdSize;
				if ((ret = ES_GetStoredTMDSize(titleID, &tmdSize)) < 0) {
					printf("ES_GetStoredTMDSize failed (%i), is the title installed..?\n", ret);
					break;
				}

				stoppads();
				ISFS_Deinitialize();
				printf("Failed? (%i)\n", WII_LaunchTitle(titleID));
				initpads();
				ISFS_Initialize();
			} break;

			case 3:
			case 4:
			case 5:
			case 6: {
				ret = wadUninstall(wad, option - 3);
				if (ret < 0) printf("wadUninstall() failed (%i)\n%s\n", ret, wad_strerror(ret));
			} break;

			case 7: {
				char path[PATH_MAX];
				strcpy(path, filepath);
				*(strrchr(path, '.')) = 0;
				ret = wadExtract(wad, path);
				if (ret < 0) printf("wadExtract() failed (%i)\n%s\n", ret, wad_strerror(ret));
			} break;
				
			default: puts("Not implemented");
		}

		puts("Press any button to continue.");
		wait_button(0);
	}

	wadFree(wad);
	return 0;
}

int main(int argc, char* argv[]) {
	__exception_setreload(15);

	puts("Loading...\n");

	if (!(patch_ahbprot_reset() && patch_isfs_permissions() && patch_ios_verify())) {
		puts("failed to apply IOS patches! Exiting in 5s...");
		sleep(5);
		return *(uint32_t*)0xCD800064;
	}

	initpads();
	ISFS_Initialize();
	AES_Init();
	if (sysmenu_process() < 0) goto error;

	if (!hasPriiloader()) {
		puts("\x1b[30;1mPlease install Priiloader...\x1b[39m");
		sleep(1);

		puts("Press A to continue.");
		wait_button(WPAD_BUTTON_A);
	}

	while (FATMount()) {
		sleep(2);
		SelectFileMenu("Select a .wad file.", "wad", selectWADFile, isWADFile, 0);
		FATUnmount();
	}

error:
	ISFS_Deinitialize();
	AES_Close();
	puts("Press HOME to exit.");
	wait_button(WPAD_BUTTON_HOME);
	stoppads();
	return 0;
}
