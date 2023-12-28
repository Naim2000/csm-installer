#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <gccore.h>
#include <ogc/es.h>
#include <wiiuse/wpad.h>

#include "video.h"
#include "iospatch.h"
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

bool isCSMfile(const char* name) {
	return hasFileExtension(name, "csm") || hasFileExtension(name, "app");
}

int main(int argc, char* argv[]) {
	int ret;
	char* file = NULL;

	__exception_setreload(15);

	puts("Loading...\n");

	if (patchIOS(false) < 0) {
		puts("failed to apply IOS patches! Exiting in 5s...");
		sleep(5);
		return 0xCD800064;
	}

	initpads();

	ISFS_Initialize();

	if (sysmenu_process() < 0)
		goto error;

	sleep(2);
	scanpads();

	if (!mountSD() && !mountUSB()) {
		puts("Unable to mount a storage device...");
		goto error;
	}

	if (!hasPriiloader()) {
		printf("\x1b[30;1mPlease install Priiloader...\x1b[39m\n\n");
		sleep(1);

		puts("Press A to continue.");
		wait_button(WPAD_BUTTON_A);
	}

	for (;;) {
		file = SelectFileMenu("Select a .wad file.", "WAD", isWADFile);
		clear();

		if (!file) {
			perror("SelectFileMenu failed");
			break;
		}

		printf("%s\n", file);
		wad_t* wad = wadInit(file);
		if (!wad) {
			puts("wadInit() failed");
			wait_button(0);
			continue;
		}

		fseek(wad->fp, 0, SEEK_END);
		size_t fsize = ftell(wad->fp);
		rewind(wad->fp);

		printf("\nFile size: 0x%x (%u)\n\n", fsize, fsize);

		printf(
			"Title ID : %016llx\n"
			"Revision : %hu\n"
			"IOS ver  : 0x%x (IOS%i)\n\n", wad->titleID, wad->titleVer, wad->titleIOS, wad->titleIOS);

		printf(
			"Certificates size: 0x%x (%u)\n"
			"CRL size: 0x%x (%u)\n"
			"Ticket size: 0x%x (%u)\n"
			"TMD size: 0x%x (%u)\n\n", wad->header.certsSize,  wad->header.certsSize,
			wad->header.crlSize, wad->header.crlSize,
			wad->header.tikSize, wad->header.tikSize,
			wad->header.tmdSize, wad->header.tmdSize);

		printf("Contents count: %hu\n\n", wad->contentsCount);

		printf(
			"Press +/START to continue.\n"
			"Press any other button to cancel.\n\n"
		);

		wait_button(0);
		if (!buttons_down(WPAD_BUTTON_PLUS)) {
			wadFree(wad);
			continue;
		}

		ret = wadInstall(wad);
		if (ret < 0)
			printf("wadInstall() failed (%i)\n%s\n", ret, wad_strerror(ret));
		else
			printf("\x1b[32mDone!\x1b[39m\n");

		wadFree(wad);
		puts("Press any button to continue.");
		wait_button(0);
	}

error:
	ISFS_Deinitialize();
	unmountSD();
	unmountUSB();
	puts("Press HOME to exit.");
	wait_button(WPAD_BUTTON_HOME);
	WPAD_Shutdown();
	return 0;
}
