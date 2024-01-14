#include <stdio.h>
#include <ogc/system.h>
#include <ogc/cache.h>
#include <ogc/video.h>
#include <ogc/color.h>
#include <ogc/video_types.h>
#include <ogc/gx.h>
#include <ogc/gx_struct.h>
#include <ogc/consol.h>

#include "video.h"
#include "malloc.h"

static void* xfb = NULL;
static GXRModeObj vmode = {};

// from LoadPriiloader
__attribute__((constructor))
void init_video() {
	VIDEO_Init();

	// setup view size
	VIDEO_GetPreferredMode(&vmode);
	vmode.viWidth = 672;

	// set correct middlepoint of the screen
    if (vmode.viTVMode & (VI_PAL << 2)) {
		vmode.viXOrigin = (VI_MAX_WIDTH_PAL - vmode.viWidth) / 2;
		vmode.viYOrigin = (VI_MAX_HEIGHT_PAL - vmode.viHeight) / 2;
	}
	else {
		vmode.viXOrigin = (VI_MAX_WIDTH_NTSC - vmode.viWidth) / 2;
		vmode.viYOrigin = (VI_MAX_HEIGHT_NTSC - vmode.viHeight) / 2;
	}

	size_t fbSize = VIDEO_GetFrameBufferSize(&vmode) + 0x100;
	xfb = memalign32(fbSize);
	DCInvalidateRange(xfb, fbSize);
	xfb = (void*)((uintptr_t)xfb | SYS_BASE_UNCACHED);

	VIDEO_SetBlack(true);
	VIDEO_Configure(&vmode);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	// Initialise the console
	CON_Init(xfb, (vmode.viWidth + vmode.viXOrigin - CONSOLE_WIDTH) / 2,
             (vmode.viHeight + vmode.viYOrigin - CONSOLE_HEIGHT) / 2,
             CONSOLE_WIDTH, CONSOLE_HEIGHT, CONSOLE_WIDTH * VI_DISPLAY_PIX_SZ);

	VIDEO_ClearFrameBuffer(&vmode, xfb, COLOR_BLACK);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode.viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
}

void clear() {
	VIDEO_WaitVSync();
	printf("\x1b[2J");
	fflush(stdout);
}