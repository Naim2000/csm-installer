#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <gccore.h>
#include <ogc/es.h>
#include <wiiuse/wpad.h>
#include <libpatcher.h>

#include "video.h"
#include "malloc.h"
#include "pad.h"
#include "fs.h"
#include "fatMounter.h"
#include "directory.h"
#include "sysmenu.h"
#include "theme.h"
#include "network.h"

__weak_symbol __printflike(1, 2)
void OSReport(const char* fmt, ...) {}

extern void __exception_setreload(int);

bool isCSMfile(const char* name) {
	return hasFileExtension(name, "csm") || hasFileExtension(name, "app");
}

int main(int argc, char* argv[]) {
	int ret;
	char* file = NULL;
	void* buffer = NULL;
	size_t size = 0;
	int restore = 0;

	__exception_setreload(10);

	if (argc) {
		for (int i = 0; i < argc; i++) {
			char* arg = argv[i];

			if (arg[0] != '-' && arg[0] != '/')
				continue;

			switch(arg[1]) {
				case 'i':
					file = (arg[2] == ':' || arg[2] == '=')?
						arg + 3 :
						argv[++i];
					break;
				case 'r':
					restore++;
					break;
			}
		}
	}

	puts(
		"Loading...\n"
		"Hold + to restore original theme!"
		"	\x1b[30;1m(timing untested with real Wii Remote)\x1b[39m");

	if (!patch_ahbprot_reset() || !patch_isfs_permissions()) {
		printf("\x1b[30;1mHW_AHBPROT: %08X\x1b[39m\n", *((volatile uint32_t*)0xcd800064));
		puts("failed to apply IOS patches! Exiting in 5s...");
		sleep(5);
	}

	initpads();
	ISFS_Initialize();

	if (sysmenu_process() < 0)
		goto error;

	sleep(2);
	scanpads();
	if (buttons_down(WPAD_BUTTON_PLUS)) {
		puts("\x1b[30;1mReady to redownload original theme\x1b[39m");
		restore++;
	}

	if (!FATMount()) {
		puts("Unable to mount a storage device...");
		goto error;
	}

	if (restore || (getSmPlatform() == (ThemeBase)Mini))
		DownloadOriginalTheme(getSmPlatform() == (ThemeBase)Mini);

	if (!hasPriiloader()) {
		printf("\x1b[30;1mPlease install Priiloader..!\x1b[39m\n\n");
		sleep(1);

		puts("Press A to continue.");
		wait_button(WPAD_BUTTON_A);
	}

	if (file)
		goto install;

	for (;;) {
		file = SelectFileMenu("Select a .csm or .app file.", "themes", isCSMfile);
		clear();

		if (!file) {
			perror("SelectFileMenu failed");
			goto error;
		}

		printf("\n%s\n\n", file);

		if (FAT_GetFileSize(file, &size) < 0) {
			perror("FAT_GetFileSize failed");
			sleep(2);
			continue;
		}

		printf("File size: %.2fMB\n\n", size / (float)0x100000);

		printf("Press +/START to install.\n"
				"Press any other button to cancel.\n\n");

		if (wait_button(WPAD_BUTTON_PLUS))
			break;

		size = 0;
	}

install:

	if (FAT_GetFileSize(file, &size) < 0) {
		perror("FAT_GetFileSize failed");
		goto error;
	}

	buffer = memalign32(size);
	if (!buffer) {
		printf("No memory..? (failed to allocate %u bytes)\n", size);
		goto error;
	}

	ret = FAT_Read(file, buffer, size, progressbar);
	if (ret < 0) {
		perror("FAT_Read failed");
		goto error;
	}

	InstallTheme(buffer, size);

error:
	free(buffer);
	network_deinit();
	ISFS_Deinitialize();
	FATUnmount();
	puts("Press HOME to exit.");
	wait_button(WPAD_BUTTON_HOME);
	WPAD_Shutdown();
	return 0;
}
