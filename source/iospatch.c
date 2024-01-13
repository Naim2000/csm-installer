#include "iospatch.h"

#include <stdbool.h>
#include <gctypes.h>
#include <ogc/ios.h>
#include <ogc/ipc.h>
#include <runtimeiospatch.h>

__attribute__((aligned(0x20)))
static const char* devDolphin = "/dev/dolphin";
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
	if (isDolphin())
		return 0;
	else
		return IosPatch_FULL(nand_permissions, false, IOS_GetVersion());
}
