#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <gccore.h>
#include <ogc/es.h>
#include <wiiuse/wpad.h>

#include "video.h"
#include "iospatch.h"
#include "pad.h"
#include "fs.h"
#include "fatMounter.h"
#include "directory.h"
#include "sysmenu.h"
#include "theme.h"
#include "network.h"

void __exception_setreload(int);
void* memalign(size_t, size_t);
unsigned int sleep(unsigned int);
[[gnu::weak]] void OSReport(const char* fmt, ...) {}

bool isCSMfile(const char* name) {
	return(fileext(name) && (strequal(fileext(name), "app") || strequal(fileext(name), "csm")));
}

int main(int argc, char* argv[]) {
	int ret;
	char* file = NULL;
	unsigned char* buffer = NULL;
	size_t size = 0;
	int restore = 0;

	__exception_setreload(15);

	if (argc) {
		for(int i = 0; i < argc; i++) {
			char* arg = argv[i];

			if (arg[0] != '-')
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

	if (restore || buttons_down(WPAD_BUTTON_PLUS)) {
		ret = InstallOriginalTheme();
		goto error;
	}

	if (!hasPriiloader()) {
		printf("\x1b[30;1mPlease install Priiloader...\x1b[39m\n\n");
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

		printf(
			"File size: %.2fMB\n\n", size / 1048576.0f);


		printf("Press +/START to install.\n"
				"Press any other button to cancel.\n\n");

		wait_button(0);
		if (buttons_down(WPAD_BUTTON_PLUS))
			break;

		size = 0;
	}

install:

	if (!size && FAT_GetFileSize(file, &size) < 0) {
		perror("FAT_GetFileSize failed");
		goto error;
	}

	buffer = memalign(0x20, size);
	if (!buffer) {
		printf("No memory..? (failed to allocate %zu bytes)\n", size);
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
	unmountSD();
	unmountUSB();
	puts("\nPress HOME to exit.");
	wait_button(WPAD_BUTTON_HOME);
	WPAD_Shutdown();
	return 0;
}
