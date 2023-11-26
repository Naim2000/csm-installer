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
[[gnu::weak]] void OSReport(const char* fmt, ...) {};

bool isCSMfile(const char* name) {
	return(fileext(name) && (strequal(fileext(name), "app") || strequal(fileext(name), "csm")));
}

int main(int argc, char* argv[]) {
	int ret;
	char* file = NULL;
	unsigned char* buffer = NULL;
	size_t filesize = 0;

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
		puts("failed to apply IOS patches...");
		goto error;
	}

	if (!fatInitDefault()) {
		puts("Unable to mount a storage device...");
		goto error;
	}

	ISFS_Initialize();

	initpads();

	if (sysmenu_process() < 0)
		goto error;

	sleep(2);
	scanpads();
	if (buttons_down(WPAD_BUTTON_PLUS))
		restore++;

	if (restore) {
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

		printf("Installing theme %s. Is this OK?\n"
				"Press +/START to confirm.\n"
				"Press any other button to cancel.\n", file);

		wait_button(0);
		if (buttons_down(WPAD_BUTTON_PLUS))
			break;
	}

install:
	ret = FAT_Read(file, &buffer, &filesize, progressbar);
	if (ret < 0) {
		printf("failed! (%d)\n", ret);
		goto error;
	}

	InstallTheme(buffer, filesize);

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
