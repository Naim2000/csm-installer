#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

#include "fatMounter.h"
#include "directory.h"

[[gnu::weak]] void OSReport([[maybe_unused]] const char* fmt, ...);

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

[[gnu::constructor]] void init_video(int row, int col) {
	VIDEO_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
}

bool isExecutable(const char* name, u8 flags) {
	return (flags & 0x01) || (fileext(name) && (strequal(fileext(name), "dol") || strequal(fileext(name), "elf")));
}

int main(int argc, char* argv[]) {
	WPAD_Init();
	if (!mountSD() && !mountUSB()) {
		printf("Unable to mount a storage device...\n");
		goto error;
	}

	char* file = SelectFileMenu("Directory.", &isExecutable);
	printf("%s\n", file);
	if (strequal(fileext(file), "txt")) puts("this is a text file, should read it");

error:
	while(!SYS_ResetButtonDown());
	unmountSD();
	unmountUSB();
	return 0;
}
