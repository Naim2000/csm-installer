#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include <ogc/usbstorage.h>

#include "fatMounter.h"
#include "video.h"
#include "directory.h"
#include "pad.h"

// Inspired by YAWM ModMii Edition
typedef struct {
	const char* friendlyName;
	const char* name;
	const DISC_INTERFACE* disk;
	long mounted;
} FATDevice;

#define NUM_DEVICES 2
static FATDevice devices[NUM_DEVICES] = {
	{ "Wii SD card slot",			"sd",	&__io_wiisd },
	{ "USB mass storage device",	"usb",	&__io_usbstorage},
};

static FATDevice* active = NULL;

bool FATMount() {
	FATDevice* attached[NUM_DEVICES] = {};
	const char* devNames[NUM_DEVICES] = {};
	int i = 0;

	for (FATDevice* dev = devices; dev < devices + NUM_DEVICES; dev++) {
		dev->disk->startup();
		if (dev->disk->isInserted()) {
		//	printf("[+]	Device detected:	\"%s\"\n", dev->friendlyName);
			attached[i] = dev;
			devNames[i++] = dev->friendlyName;
		}
		else dev->disk->shutdown();
	}

	if (!i) {
		puts("\x1b[30;1mNo storage devices are attached...\x1b[39m");
		return false;
	}

	puts("Choose a device to mount.");
	int index = QuickActionMenu(i, devNames);
	if (!index) return false;

	FATDevice* target = attached[--index];
	printf("Mounting %s:/ ... ", target->name);
	if ((target->mounted = fatMountSimple(target->name, target->disk))) {
		puts("OK!");
		active = target;
	}
	else {
		puts("Failed!");
		target->disk->shutdown();
	}
	
	return target->mounted;
}

void FATUnmount() {
	for (FATDevice* dev = devices; dev < devices + NUM_DEVICES; dev++) {
		if (dev->mounted) {
			fatUnmount(dev->name);
			dev->disk->shutdown();
			dev->mounted = false;
		}
	}
	
	active = NULL;
}

const char* GetActiveDeviceName() { return active? active->name : NULL; }

