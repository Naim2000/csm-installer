#include "iospatch.h"

#include <stdbool.h>
#include <gctypes.h>
#include <ogc/ios.h>
#include <ogc/ipc.h>
#include <runtimeiospatch.h>

static const char* devDolphin [[gnu::aligned(0x20)]] = "/dev/dolphin";
static int dolphin_fd = ~0;

bool isDolphin() {
	if (!~dolphin_fd) {
		dolphin_fd = IOS_Open(devDolphin, 0);
		if (dolphin_fd > 0)
			IOS_Close(dolphin_fd);
	}
	return dolphin_fd > 0;
}

int patchIOS() {

		return IosPatch_FULL(nand_permissions, true, IOS_GetVersion());
}
