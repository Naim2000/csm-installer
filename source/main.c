#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <ogc/es.h>
#include <wiiuse/wpad.h>
#include <runtimeiospatch.h>

#include "pad.h"
#include "iospatch.h"
#include "fatMounter.h"
#include "fs.h"
#include "directory.h"
#include "sysmenu.h"

void* memalign(size_t, size_t);
unsigned int sleep(unsigned int);
[[gnu::weak]] void OSReport(const char* fmt, ...) {};

bool isCSMfile(const char* name, u8 flags) {
	return (flags & 0x01) || (fileext(name) && (strequal(fileext(name), "app") || strequal(fileext(name), "csm")));
}

int main(int argc, char* argv[]) {
	int ret;

	puts("Loading...");

	initpads();

	if (patchIOS(false) < 0) {
		puts("failed to apply IOS patches...");
		goto error;
	}

	if (!mountSD() && !mountUSB()) {
		puts("Unable to mount a storage device...");
		goto error;
	}

	ISFS_Initialize();

	if (sysmenu_process() < 0)
		goto error;

	if (!getArchiveCid()) {
		puts("Failed to identify system menu archive!");
		goto error;
	}

	if (!hasPriiloader()) {
		puts("Please install Priiloader...");
		sleep(2);

		puts("Press A to continue");
		wait_button(WPAD_BUTTON_A);
	}

	char* file = SelectFileMenu("Select an .app or .csm file.", isCSMfile);
	if (!file) {
		perror("Failed to get csm file to install");
		goto error;
	}

	unsigned char* buffer = NULL;
	size_t filesize = 0;

	printf("\n%s\n", file);
	ret = FAT_Read(file, &buffer, &filesize, progressbar);
	if (ret < 0) {
		printf("error. (%d)\n", ret);
		goto error;
	}

	sprintf(strrchr(sysmenu_filepath, '/'), "/%08x.app", getArchiveCid());
	printf("\n%s\n", sysmenu_filepath);
	ret = FS_Write(sysmenu_filepath, buffer, filesize, progressbar);
	if (ret < 0) {
		printf("error. (%d)\n", ret);
		goto error;
	}
	printf("\n\n");


error:
	ISFS_Deinitialize();
	unmountSD();
	unmountUSB();
	puts("Press HOME to exit.");
	wait_button(WPAD_BUTTON_HOME);
	WPAD_Shutdown();
	return 0;
}
