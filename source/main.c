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
#include "theme.h"

void* memalign(size_t, size_t);
unsigned int sleep(unsigned int);
[[gnu::weak]] void OSReport(const char* fmt, ...) {};

bool isCSMfile(const char* name, u8 flags) {
	return (flags & 0x01) || (fileext(name) && (strequal(fileext(name), "app") || strequal(fileext(name), "csm")));
}

int main(int argc, char* argv[]) {
	int ret;
	char* file;
	unsigned char* buffer = NULL;
	size_t filesize = 0;

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

	if (argc)
		file = argv[0];
	else
		file = SelectFileMenu("csm-installer 0.0 // Select a .csm or .app file.", isCSMfile);

	printf("%s\n", file);
	ret = FAT_Read(file, &buffer, &filesize, progressbar);
	if (ret < 0) {
		printf("error. (%d)\n", ret);
		goto error;
	}

	if (SignedTheme(buffer, filesize))
		printf("\x1b[36mThis theme was signed by wii-themer.\x1b[39m\n\n");
	else
		printf("This theme was not signed by wii-themer..!\n\n");

	version_t themeversion = GetThemeVersion(buffer, filesize);
	if (themeversion.region != getSmRegion()) {
		printf("\x1b[41;30mIncompatible theme!\x1b[40;39m\nTheme region : %c\nSystem region: %c\n", themeversion.region, getSmRegion());
		goto error;
	}
	else if (themeversion.major != getSmVersionMajor()) {
		printf("\x1b[41;30mIncompatible theme!\x1b[40;39m\nTheme major version : %c\nSystem major version: %c\n", themeversion.major, getSmVersionMajor());
	}

	sprintf(strrchr(sysmenu_filepath, '/'), "/%08x.app", getArchiveCid());
	printf("%s\n", sysmenu_filepath);
	ret = FS_Write(sysmenu_filepath, buffer, filesize, progressbar);
	if (ret < 0) {
		printf("error. (%d)\n", ret);
		goto error;
	}

error:
	free(buffer);
	ISFS_Deinitialize();
	unmountSD();
	unmountUSB();
	puts("\nPress HOME to exit.");
	wait_button(WPAD_BUTTON_HOME);
	WPAD_Shutdown();
	return 0;
}
